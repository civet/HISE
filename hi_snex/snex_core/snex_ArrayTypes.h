/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licences for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licencing:
*
*   http://www.hartinstruments.net/hise/
*
*   HISE is based on the JUCE library,
*   which also must be licenced for commercial applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once

#include <cstring>

#include <type_traits>
#include <initializer_list>
#include <nmmintrin.h>
#include <stdint.h>

namespace snex {
namespace Types {


struct DSP
{
	template <class WrapType, class T> static float interpolate(T& data, float v)
	{
		static_assert(std::is_same<T, typename WrapType::ParentType>(), "parent type mismatch ");

		auto floorValue = (int)v;

		WrapType lower = { (int)v };
		WrapType upper = { lower.value + 1 };

		int lowerIndex = lower.get(data);
		int upperIndex = upper.get(data);

		auto ptr = data.begin();
		auto lowerValue = ptr[lowerIndex];
		auto upperValue = ptr[upperIndex];
		auto alpha = v - (float)floorValue;
		const float invAlpha = 1.0f - alpha;
		return invAlpha * lowerValue + alpha * upperValue;
	}
};

/** A fixed-size array type for SNEX. 

	The span type is an iteratable compile-time array.

*/
template <class T, int MaxSize> struct span
{
	using DataType = T;
	using Type = span<T, MaxSize>;

	static constexpr int s = MaxSize;

	struct wrapped
	{
		using ParentType = Type;

		wrapped& operator=(int v) { value = v; return *this; }

		int get(const Type& obj) const
		{
			auto i = value % MaxSize;
			return i;
		}

		int value = 0;
	};

	struct zeroed
	{
		using ParentType = Type;

		zeroed& operator=(int v) { value = v; return *this; }

		int get(const Type& obj) const
		{
			if (isPositiveAndBelow(value, MaxSize - 1))
				return value;

			return 0;
		}

		int value = 0;
	};

	struct clamped
	{
		using ParentType = Type;

		clamped& operator=(int v) { value = v; return *this; }

		int get(const Type& obj) const
		{
			return jlimit(0, MaxSize - 1, value);
		}

		int value = 0;
	};

	struct unsafe
	{
		using ParentType = Type;

		unsafe& operator=(int v) { value = v; return *this; }

		int get(const Type& obj) const
		{
			return value;
		}

		int value = 0;
	};

	template <class IndexType> IndexType index(int value)
	{
		IndexType obj = { value };
		auto v = obj.get(*this);
		obj.value = v;
		return obj;
	}

	span()
	{
		memset(data, 0, sizeof(T)*MaxSize);
	}

	span(const std::initializer_list<T>& l)
	{
		if (l.size() == 1)
		{
			for (int i = 0; i < MaxSize; i++)
			{
				data[i] = *l.begin();
			}
		}
		else
			memcpy(data, l.begin(), sizeof(T)*MaxSize);

	}

	static Type& fromExternalData(T* data, int numElements)
	{
		jassert(numElements <= MaxSize);
		return *reinterpret_cast<Type*>(data);
	}

	static constexpr size_t getSimdSize()
	{
		static_assert(isSimdable(), "not SIMDable");

		if (std::is_same<T, float>())
			return 4;
		else
			return 2;
	}

	Type& operator=(const T& t)
	{
		static_assert(isSimdable(), "Can't add non SIMDable types");

		constexpr int numLoop = MaxSize / getSimdSize();
		int i = 0;

		float t_ = (float)t;

		if (std::is_same<T, float>())
		{
			auto dst = (float*)data;

			for (int i = 0; i < numLoop; i++)
			{
				auto v = _mm_load_ps1(&t_);
				_mm_store_ps(dst, v);

				dst += getSimdSize();
			}
		}

		return *this;
	}

	operator DataType()
	{
		if (MaxSize == 1)
			return *begin();
	}

	Type& operator+=(const T& scalar)
	{
		*this + scalar;
		return *this;
	}

	Type& operator+(const T& scalar)
	{
		static_assert(isSimdable(), "Can't add non SIMDable types");

		constexpr int numLoop = MaxSize / getSimdSize();
		auto dst = (float*)data;
		auto sc = (float)scalar;

		auto v = _mm_load_ps1(&scalar);

		for (int i = 0; i < numLoop; i++)
		{
			auto r = _mm_add_ps(_mm_load_ps(dst), v);
			_mm_store_ps(dst, r);

			dst += getSimdSize();
		}

		return *this;
	}

	Type& operator=(const Type& other)
	{
		static_assert(isSimdable(), "Can't add non SIMDable types");

		constexpr int numLoop = MaxSize / getSimdSize();
		auto src = (float*)other.data;
		auto dst = (float*)data;

		for (int i = 0; i < numLoop; i++)
		{
			auto v = _mm_load_ps((float*)src);
			_mm_store_ps(dst, v);

			src += getSimdSize();
			dst += getSimdSize();
		}

		return *this;
	}

	Type& operator+ (const Type& other)
	{
		auto dst = (float*)data;
		auto src = (float*)other.data;
		constexpr int numLoop = MaxSize / getSimdSize();

		for (int i = 0; i < numLoop; i++)
		{
			auto v = _mm_load_ps(dst);
			auto r = _mm_add_ps(v, _mm_load_ps(src));
			_mm_store_ps(dst, r);

			dst += getSimdSize();
			src += getSimdSize();
		}

		return *this;
	}

	Type& operator +=(const Type& other)
	{
		static_assert(isSimdable(), "Can't add non SIMDable types");

		constexpr int numLoop = MaxSize / getSimdSize();
		int i = 0;

		auto dst = (float*)data;
		auto src = (float*)other.data;

		for (int i = 0; i < numLoop; i++)
		{
			auto v = _mm_load_ps(dst);
			auto r = _mm_add_ps(v, _mm_load_ps(src));
			_mm_store_ps(dst, r);

			dst += getSimdSize();
			src += getSimdSize();
		}

		return *this;
	}

	T accumulate() const
	{
		T v = T(0);

		for (int i = 0; i < MaxSize; i++)
		{
			v += data[i];
		}

		return v;
	}

	static constexpr bool isSimdType()
	{
		return (std::is_same<T, float>() && MaxSize == 4) ||
			(std::is_same<T, double>() && MaxSize == 2);
	}

	static constexpr bool isSimdable()
	{
		return (std::is_same<T, float>() && MaxSize % 4 == 0) ||
			(std::is_same<T, double>() && MaxSize % 2 == 0);
	}

	bool isAlignedTo16Byte() const
	{
		return isAlignedTo16Byte(*this);
	}


	template <class WrapType> float interpolate(float index)
	{
		return DSP::interpolate<WrapType>(*this, index);
	}

	template <class ObjectType> static bool isAlignedTo16Byte(ObjectType& d)
	{
		return reinterpret_cast<uint64_t>(d.begin()) % 16 == 0;
	}

	template <int ChannelAmount> span<span<T, MaxSize / ChannelAmount>, ChannelAmount>& split()
	{
		static_assert(MaxSize % ChannelAmount == 0, "Can't split with slice length ");

		return *reinterpret_cast<span<span<T, MaxSize / ChannelAmount>, ChannelAmount>*>(this);
	}

	span<span<float, 4>, MaxSize / 4>& toSimd()
	{
		using Type = span<span<float, 4>, MaxSize / 4>;

		static_assert(isSimdable(), "is not SIMDable");
		jassert(isAlignedTo16Byte());

		return *reinterpret_cast<Type*>(this);
	}

	template <class IndexType> const T& operator[](const IndexType& i) const
	{
		auto index = i.get(*this);
		return data[index];
	}

	template <class IndexType> T& operator[](const IndexType& i)
	{
		auto index = i.get(*this);
		return data[index];
	}

	const T& operator[](int index) const
	{
		return data[index];
	}

	/** Morphs any pointer of the data type into this type. */
	constexpr static Type& as(T* ptr)
	{
		return *reinterpret_cast<Type*>(ptr);
	}

	T& operator[](int index)
	{
		return data[index];
	}

	T* begin() const
	{
		return const_cast<T*>(data);
	}

	T* end() const
	{
		return const_cast<T*>(data + MaxSize);
	}

	void fill(const T& value)
	{
		for (auto& v : *this)
			v = value;
	}

	constexpr int size()
	{
		return MaxSize;
	}

	static constexpr int alignment()
	{
		if (MaxSize < 4)
			return sizeof(T);
		else
			return 16;
	}

	alignas(alignment()) T data[MaxSize];
};

using float4 = span<float, 4>;

template <class T> struct dyn
{
	using Type = dyn<T>;
	using DataType = T;

	struct wrapped
	{
		using ParentType = Type;

		wrapped(int v) : value(v) {};
		wrapped& operator=(int v) { value = v; return *this; }

		wrapped moved(int delta) const
		{
			wrapped t;
			t.value = value + delta;
			return t;
		}

		bool valid(dyn<T>& obj) const
		{
			return get(obj) == value;
		}

		int operator++()
		{
			return value++;
		}

		int get(const dyn<T>& obj) const
		{
			auto i = value % obj.size();
			return i;
		}

		int value = 0;
	};

	struct zeroed
	{
		using ParentType = Type;

		zeroed(int v) : value(v) {};
		zeroed& operator=(int v) { value = v; return *this; }

		bool valid(dyn<T>& obj) const
		{
			return get(obj) == value;
		}

		int get(const dyn<T>& obj) const
		{
			if (isPositiveAndBelow(value, obj.size()))
				return value;

			return 0;
		}

		int operator++()
		{
			return value++;
		}

		int value = 0;
	};

	struct clamped
	{
		using ParentType = Type;

		clamped() = default;
		clamped(int v) : value(v) {};
		clamped& operator=(int v) { value = v; return *this; }

		bool valid(dyn<T>& obj) const
		{
			return get(obj) == value;
		}

		int get(const dyn<T>& obj) const
		{
			return jlimit(0, obj.size(), value);
		}

		int operator++()
		{
			return value++;
		}

		int value = 0;
	};

	struct unsafe
	{
		using ParentType = Type;

		unsafe() = default;
		unsafe(int v) : value(v) {};
		unsafe& operator=(int v) { value = v; return *this; }

		bool valid(dyn<T>& obj) const
		{
			return get(obj) == value;
		}

		int get(const dyn<T>& obj) const
		{
			return value;
		}

		int operator++()
		{
			return value++;
		}

		int value = 0;
	};

	dyn() :
		data(nullptr),
		size_(0)
	{}


	template<class Other> dyn(Other& o) :
		data(o.begin()),
		size_(o.end() - o.begin())
	{
		static_assert(std::is_same<DataType, Other::DataType>(), "not same data type");
	}

	template<class Other> dyn(Other& o, size_t s_) :
		data(o.begin()),
		size_(s_)
	{
		static_assert(std::is_same<DataType, Other::DataType>(), "not same data type");
	}

	dyn(juce::HeapBlock<T>& d, size_t s_):
		data(d.get()),
		size_(s_)
	{}

	dyn(T* data_, size_t s_) :
		data(data_),
		size_(s_)
	{}

	template<class Other> dyn(Other& o, size_t s_, size_t offset) :
		data(o.begin() + offset),
		size_(s_)
	{
		auto fullSize = o.end() - o.begin();
		jassert(offset + size() < fullSize);
	}

	dyn<float4> toSimd() const
	{
		dyn<float4> rt;

		jassert(size() % 4 == 0);

		rt.data = reinterpret_cast<float4*>(begin());
		rt.size_ = size() / 4;

		return rt;
	}

	template <int NumChannels> span<dyn<T>, NumChannels> split()
	{
		span<dyn<T>, NumChannels> r;

		int newSize = size() / NumChannels;

		T* d = data;

		for (auto& e : r)
		{
			e = dyn<T>(d, newSize);
			d += newSize;
		}

		return r;
	}

	template <class WrapType> float interpolate(float index)
	{
		return DSP::interpolate<WrapType>(*this, index);
	}

	template <class IndexType> const T& operator[](IndexType& t) const
	{
		auto index = t.get(*this);
		return data[index];
	}

	template <class IndexType> T& operator[](IndexType& t)
	{
		auto index = t.get(*this);
		return data[index];
	}

	const T& operator[](int index) const
	{
		return data[index];
	}

	template <class Other> bool valid(Other& t)
	{
		return t.valid(*this);
	}

	T& operator[](int index)
	{
		return data[index];
	}

	T* begin() const
	{
		return const_cast<T*>(data);
	}

	T* end() const
	{
		return const_cast<T*>(data) + size();
	}

	bool isEmpty() const noexcept { return size() == 0; }

	int size() const noexcept { return size_; }

	template <typename OtherContainer> void copyTo(OtherContainer& t)
	{
		jassert(size() <= t.size());
		int numBytesToCopy = size() * sizeof(T);
		memcpy(t.begin(), begin(), numBytesToCopy);
	}

	template <typename OtherContainer> void copyFrom(const OtherContainer& t)
	{
		jassert(size() >= t.size());
		int numBytesToCopy = t.size() * sizeof(T);
		memcpy(begin(), t.begin(), numBytesToCopy);
	}

	int unused = Types::ID::Block;
	int size_ = 0;
	T* data;

};


template <typename T> struct heap
{
	using Type = heap<T>;
	using DataType = T;

	int size() const noexcept { return size_; }

	bool isEmpty() const noexcept { return size() == 0; }

	void setSize(int numElements)
	{
		if (numElements != size())
		{
			data.allocate(numElements, true);
			size_ = numElements;
		}
	}

	T& operator[](int index)
	{
		return *(begin() + index);
	}

	const T& operator[](int index) const
	{
		return *(begin() + index);
	}

	T* begin() const { return data.get();  }
	T* end() const { return data + size(); }

	template <typename OtherContainer> void copyTo(OtherContainer& t)
	{
		jassert(size() <= t.size());
		int numBytesToCopy = size() * sizeof(T);
		memcpy(t.begin(), begin(), numBytesToCopy);
	}

	template <typename OtherContainer> void copyFrom(const OtherContainer& t)
	{
		jassert(size() >= t.size());
		int numBytesToCopy = t.size() * sizeof(T);
		memcpy(begin(), t.begin(), numBytesToCopy);
	}

	int unused = Types::ID::Block;
	int size_ = 0;
	juce::HeapBlock<T> data;
};

namespace Interleaver
{


static void interleave(float* src, int numFrames, int numChannels)
{
	size_t numBytes = sizeof(float) * numChannels * numFrames;

	float* dst = (float*)alloca(numBytes);

	for (int i = 0; i < numFrames; i++)
	{
		for (int j = 0; j < numChannels; j++)
		{
			auto targetIndex = i * numChannels + j;
			auto sourceIndex = j * numFrames + i;

			dst[targetIndex] = src[sourceIndex];
		}
	}

	memcpy(src, dst, numBytes);
}


static void interleaveRaw(float* src, int numFrames, int numChannels)
{
	interleave(src, numFrames, numChannels);
}

template <class T, int NumChannels> static auto interleave(span<dyn<T>, NumChannels>& t)
{
	jassert(isContinousMemory(t));

	int numFrames = t[0].size;

	static_assert(std::is_same<float, T>(), "must be float");

	// [ [ptr, size], [ptr, size] ]
	// => [ ptr, size ] 

	using FrameType = span<T, NumChannels>;


	auto src = reinterpret_cast<float*>(t[0].begin());
	interleaveRaw(src, numFrames, NumChannels);

	return dyn<FrameType>(reinterpret_cast<FrameType*>(src), numFrames);
}

/** Interleaves the float data from a dynamic array of frames.

	dyn_span<T>(numChannels) => span<dyn_span<T>, NumChannels>
*/
template <class T, int NumChannels> static auto interleave(dyn<span<T, NumChannels>>& t)
{
	jassert(isContinousMemory(t));

	int numFrames = t.size;

	span<dyn<T>, NumChannels> d;

	float* src = t.begin()->begin();


	interleave(src, NumChannels, numFrames);

	for (auto& r : d)
	{
		r = dyn<T>(src, numFrames);
		src += numFrames;
	}

	return d;
}

template <class T, int NumFrames, int NumChannels> static auto& interleave(span<span<T, NumFrames>, NumChannels>& t)
{
	static_assert(std::is_same<float, T>(), "must be float");
	jassert(isContinousMemory(t));

	using ChannelType = span<T, NumChannels>;

	int s1 = sizeof(span<ChannelType, NumFrames>);
	int s2 = sizeof(span<span<T, NumFrames>, NumChannels>);



	auto src = reinterpret_cast<float*>(t.begin());
	interleave(src, NumFrames, NumChannels);
	return *reinterpret_cast<span<ChannelType, NumFrames>*>(&t);
}

template <class T> static bool isContinousMemory(const T& t)
{
	using ElementType = typename T::DataType;

	auto ptr = t.begin();
	auto e = t.end();

	auto elementSize = sizeof(ElementType);

	auto size = (e - ptr) * elementSize;

	auto realSize = reinterpret_cast<uint64_t>(e) - reinterpret_cast<uint64_t>(ptr);

	return realSize == size;
}


template <class DataType> static dyn<DataType> slice(const dyn<DataType>& src, int start, int size = -1)
{
	if (size == -1)
		size = src.size - start;

	using ClampType = typename dyn<DataType>::clamped;

	ClampType clampedStart = start;
	ClampType clampedSize = start + size;

	dyn<DataType> c;
	c.data = src.begin() + clampedStart.get(src);
	c.size = clampedSize.get(src) - clampedStart.get(src);
	return c;
}

template <class DataType, int MaxSize> static dyn<DataType> slice(const span<DataType, MaxSize>& src, int start, int size = -1)
{
	using SpanType = span<DataType, MaxSize>;

	if (size == -1)
		size = MaxSize - start;

	typename SpanType::clamped clampedStart = { start };
	typename SpanType::clamped clampedSize = { start + size };

	dyn<DataType> c;
	c.data = src.begin() + clampedStart.get(src);
	c.size_ = clampedSize.get(src) - clampedStart.get(src);
	return c;
}

template <int Start, int Size, class DataType, int MaxSize> static span<DataType, Size>& slice(const span<DataType, MaxSize>& src)
{
	using SpanType = span<DataType, Size>;

	constexpr int SizeToUse = Size == -1 ? MaxSize - Start : Size;

	typename SpanType::clamped clampedStart = Start;
	typename SpanType::clamped clampedSize = Start + SizeToUse;

	return *reinterpret_cast<SpanType*>(src.begin() + Start);
}


};




}
}
