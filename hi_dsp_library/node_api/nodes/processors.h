/*  ===========================================================================
 *
 *   This file is part of HISE.
 *   Copyright 2016 Christoph Hart
 *
 *   HISE is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   HISE is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Commercial licenses for using HISE in an closed source project are
 *   available on request. Please visit the project's website to get more
 *   information about commercial licensing:
 *
 *   http://www.hise.audio/
 *
 *   HISE is based on the JUCE library,
 *   which also must be licenced for commercial applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

#pragma once

namespace scriptnode
{
using namespace juce;
using namespace hise;

namespace scriptnode_initialisers
{
struct oversample;
}

using namespace snex::Types;



/** The wrap namespace contains templates that wrap around an existing node type and change the processing.

	Examples are:

	- fixed channel amount: wrap::fix
	- fixed upper block size: wrap::fix_block
	- frame based processing: wrap::frame_x

	Most of the templates just hold an object of the given type and forward most callbacks to its inner object
	while adding the additional functionality where required.
*/
namespace wrap
{



/** This namespace holds all the logic of custom node wrappers in static functions so it can be used
	by the SNEX jit compiler as well as the C++ classes.

*/
namespace static_functions
{

namespace prototypes
{
template <typename ProcessDataType> using process = void(*)(void*, ProcessDataType&);
typedef void(*prepare)(void*, PrepareSpecs);
typedef void(*handleHiseEvent)(void*, HiseEvent&);
}

template <int ChannelAmount> struct frame
{
	static void prepare(void* obj, prototypes::prepare f, const PrepareSpecs& ps)
	{
		f(obj, ps.withNumChannelsT<ChannelAmount>().withBlockSize(1));
	}
};

template <int BlockSize> struct fix_block
{
	static void prepare(void* obj, prototypes::prepare f, const PrepareSpecs& ps)
	{
		f(obj, ps.withBlockSizeT<BlockSize>());
	}

	template <typename ProcessDataType> static void process(void* obj, prototypes::process<ProcessDataType> pf, ProcessDataType& data)
	{
		int numToDo = data.getNumSamples();

		if (numToDo < BlockSize)
			pf(obj, data);
		else
		{
			// We need to forward the HiseEvents to the chunks as there might
			// be a event node sitting in there...
			static constexpr bool IncludeHiseEvents = true;

			ChunkableProcessData<ProcessDataType, IncludeHiseEvents> cpd(data);

			while (cpd)
			{
				int numThisTime = jmin(BlockSize, cpd.getNumLeft());
				auto c = cpd.getChunk(numThisTime);
				pf(obj, c.toData());
			}

#if 0
			float* tmp[ProcessDataType::getNumFixedChannels()];

			for (int i = 0; i < data.getNumChannels(); i++)
				tmp[i] = data[i].data;

			while (numToDo > 0)
			{
				int numThisTime = jmin(BlockSize, numToDo);
				ProcessDataType copy(tmp, numThisTime, data.getNumChannels());
				copy.copyNonAudioDataFrom(data);
				typedFunction(obj, copy);

				for (int i = 0; i < data.getNumChannels(); i++)
					tmp[i] += numThisTime;

				numToDo -= numThisTime;
			}
#endif
		}
	}
};



/** Continue... */
struct event
{
	template <typename ProcessDataType> static void process(void* obj, prototypes::process<ProcessDataType> pf, prototypes::handleHiseEvent ef, ProcessDataType& d)
	{
		auto events = d.toEventData();

		if (events.size() > 0)
		{
			// We don't need the event in child nodes as it should call 
			// handleHiseEvent recursively from here...
			static constexpr bool IncludeHiseEvents = false;

			ChunkableProcessData<ProcessDataType, IncludeHiseEvents> aca(d);

			int lastPos = 0;

			for (auto& e : events)
			{
				if (e.isIgnored())
					continue;

				auto samplePos = e.getTimeStamp();

				const int numThisTime = jmin(aca.getNumLeft(), samplePos - lastPos);

				if (numThisTime > 0)
				{
					auto c = aca.getChunk(numThisTime);
					pf(obj, c.toData());
				}

				ef(obj, e);
				lastPos = samplePos;
			}

			if (aca)
			{
				auto c = aca.getRemainder();
				pf(obj, c.toData());
			}
		}
		else
		{
			pf(obj, d);
		}
	}
};

}



/** This wrapper template will create a compile-time channel configuration which might
	increase the performance and inlineability.

	Usage:

	@code
	using stereo_oscillator = fix<2, core::oscillator>
	@endcode

	If you wrap a node into this template, it will call the process functions with
	the fixed size process data arguments:

	void process(ProcessData<C>& data)

	void processFrame(span<float, C>& data)

	instead of the dynamic ones.
*/
template <int C, class T> class fix
{
public:

	SN_OPAQUE_WRAPPER(fix, T);

	static const int NumChannels = C;

	/** The process data type to use for this node. */
	using FixProcessType = snex::Types::ProcessData<NumChannels>;

	/** The frame data type to use for this node. */
	using FixFrameType = snex::Types::span<float, NumChannels>;

	fix() {};

	constexpr bool isPolyphonic() const { return this->obj.getWrappedObject().isPolyphonic(); }

	static Identifier getStaticId() { return T::getStaticId(); }

	/** Forwards the callback to its wrapped object. */
	void initialise(NodeBase* n)
	{
		this->obj.initialise(n);
	}

	/** Forwards the callback to its wrapped object, but it will change the channel amount to NumChannels. */
	void prepare(PrepareSpecs ps)
	{
		ps.numChannels = NumChannels;
		this->obj.prepare(ps);
	}

	/** Forwards the callback to its wrapped object. */
	forcedinline void reset() noexcept { obj.reset(); }

	/** Forwards the callback to its wrapped object. */
	void handleHiseEvent(HiseEvent& e)
	{
		this->obj.handleHiseEvent(e);
	}

	/** Processes the given data, but will cast it to the fixed channel version.

		You must not call this function with a ProcessDataType that has less channels
		or the results will be unpredictable.

		Leftover channels will not be processed.
	*/
	template <typename ProcessDataType> void process(ProcessDataType& data) noexcept
	{
		jassert(data.getNumChannels() >= NumChannels);
		auto& fd = data.as<FixProcessType>();
		this->obj.process(fd);
	}


	/** Processes the given data, but will cast it to the fixed channel version.

		You must not call this function with a FrameDataType that has less channels
		or the results will be unpredictable.

		Leftover channels will not be processed.
	*/
	template <typename FrameDataType> forcedinline void processFrame(FrameDataType& data) noexcept
	{
		jassert(data.size() >= NumChannels);
		auto& d = FixFrameType::as(data.begin());
		this->obj.processFrame(d);
	}

	/** Forwards the callback to its wrapped object. */
	void createParameters(ParameterDataList& data)
	{
		this->obj.createParameters(data);
	}

private:

	T obj;
};


/** A wrapper around a node that allows custom initialisation.

	Just create an initialiser class that takes a reference
	to a T object and initialises it and it will do so in 
	the constructor of this class.
	
	You can use it in order to setup default values and
	add parameter connections for container nodes.
*/
template <class T, class Initialiser> class init
{
public:

	SN_SELF_AWARE_WRAPPER(init, T);

	init() : obj(), i(obj) {};

	HISE_DEFAULT_INIT(T);
	HISE_DEFAULT_PREPARE(T);
	HISE_DEFAULT_RESET(T);
	HISE_DEFAULT_HANDLE_EVENT(T);
	HISE_DEFAULT_PROCESS_FRAME(T);
	HISE_DEFAULT_PROCESS(T);

	template <int P> static void setParameter(void* obj, double value)
	{
		static_cast<init*>(obj)->setParameter<P>(value);
	}

	template <int P> void setParameter(double value)
	{
		obj.template setParameter<P>(value);
	}

	T obj;
	Initialiser i;
};


/** Wrapping a node into this template will bypass all callbacks.

	You can use it to quickly deactivate a node (or the code generator might use this
	in order to generate nodes that are bypassed.

*/
template <class T> class skip
{
public:

	SN_OPAQUE_WRAPPER(skip, T);

	void initialise(NodeBase* n)
	{
		this->obj.initialise(n);
	}

	void prepare(PrepareSpecs) {}
	void reset() noexcept {}

	template <typename ProcessDataType> void process(ProcessDataType&) noexcept {}

	void handleHiseEvent(HiseEvent&) {}

	template <typename FrameDataType> void processFrame(FrameDataType&) noexcept {}

private:

	T obj;
};

#define INTERNAL_EVENT_FUNCTION(ObjectClass) static void handleHiseEventInternal(void* obj, HiseEvent& e) { auto& typed = *static_cast<ObjectClass*>(obj); typed.handleHiseEvent(e); }

template <class T> class event
{
public:

	SN_OPAQUE_WRAPPER(event, T);

	bool isPolyphonic() const { return obj.isPolyphonic(); }

	HISE_DEFAULT_INIT(T);
	HISE_DEFAULT_PREPARE(T);
	HISE_DEFAULT_RESET(T);
	HISE_DEFAULT_HANDLE_EVENT(T);
	HISE_DEFAULT_PROCESS_FRAME(T);

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		auto p = [](void* obj, ProcessDataType& data) { static_cast<event*>(obj)->obj.process(data); };
		auto e = [](void* obj, HiseEvent& e) { static_cast<event*>(obj)->obj.handleHiseEvent(e); };
		static_functions::event::process<ProcessDataType>(this, p, e, data);
	}

	T obj;

private:
};


template <class T> class frame_x
{
public:

	SN_OPAQUE_WRAPPER(frame_x, T);

	void initialise(NodeBase* n)
	{
		obj.initialise(n);
	}

	void prepare(PrepareSpecs ps)
	{
		ps.blockSize = 1;
		obj.prepare(ps);
	}

	void handleHiseEvent(HiseEvent& e)
	{
		obj.handleHiseEvent(e);
	}

	forcedinline void reset() noexcept { obj.reset(); }

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		constexpr int C = ProcessDataType::getNumFixedChannels();

		if constexpr (ProcessDataType::isFixedChannel)
			FrameConverters::processFix<C>(&obj, data);
		else
			FrameConverters::forwardToFrame16(&obj, data);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data) noexcept
	{
		this->obj.processFrame(data);
	}

private:

	T obj;
};

/** Just a shortcut to the frame_x type. */
template <int C, typename T> using frame = fix<C, frame_x<T>>;

#if 0
template <int NumChannels, class T> class frame
{
public:

	GET_SELF_OBJECT(obj);
	GET_WRAPPED_OBJECT(obj);

	using FixProcessType = snex::Types::ProcessData<NumChannels>;
	using FrameType = snex::Types::span<float, NumChannels>;

	void initialise(NodeBase* n)
	{
		obj.initialise(n);
	}

	void prepare(PrepareSpecs ps)
	{
		ps.numChannels = NumChannels;
		obj.prepare(ps);
	}

	void handleHiseEvent(HiseEvent& e)
	{
		obj.handleHiseEvent(e);
	}

	forcedinline void reset() noexcept { obj.reset(); }

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		snex::Types::ProcessDataHelpers<NumChannels>::processFix(&obj, data);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data) noexcept
	{
		this->obj.processFrame(data);
	}

private:

	T obj;
};
#endif



struct oversample_base
{
	using Oversampler = juce::dsp::Oversampling<float>;

	oversample_base(int factor) :
		oversamplingFactor(factor)
	{};

	void prepare(PrepareSpecs ps)
	{
		jassert(lock != nullptr);

		ScopedPointer<Oversampler> newOverSampler;

		auto originalBlockSize = ps.blockSize;

		ps.sampleRate *= (double)oversamplingFactor;
		ps.blockSize *= oversamplingFactor;

		if (pCallback)
			pCallback(ps);
		
		newOverSampler = new Oversampler(ps.numChannels, (int)std::log2(oversamplingFactor), Oversampler::FilterType::filterHalfBandPolyphaseIIR, false);

		if (originalBlockSize > 0)
			newOverSampler->initProcessing(originalBlockSize);

		{
			ScopedLock sl(*lock);
			oversampler.swapWith(newOverSampler);
		}
	}

	CriticalSection* lock = nullptr;

protected:

	const int oversamplingFactor = 0;
	std::function<void(PrepareSpecs)> pCallback;
	ScopedPointer<Oversampler> oversampler;
};





template <int OversamplingFactor, class T, class InitFunctionClass=scriptnode_initialisers::oversample> class oversample: public oversample_base
{
public:

	SN_OPAQUE_WRAPPER(oversample, T);

	oversample():
		oversample_base(OversamplingFactor)
	{
		pCallback = [this](PrepareSpecs ps)
		{
			obj.prepare(ps);
		};
	}

	forcedinline void reset() noexcept 
	{
		if (oversampler != nullptr)
			oversampler->reset();

		obj.reset(); 
	}

	void handleHiseEvent(HiseEvent& e)
	{
		obj.handleHiseEvent(e);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data) noexcept
	{
		// shouldn't be called since the oversampling always has to happen on block level...
		jassertfalse;
	}

	template <typename ProcessDataType>  void process(ProcessDataType& data)
	{
		if (oversampler == nullptr)
			return;

		auto bl = data.toAudioBlock();
		auto output = oversampler->processSamplesUp(bl);

		float* tmp[NUM_MAX_CHANNELS];

		for (int i = 0; i < data.getNumChannels(); i++)
			tmp[i] = output.getChannelPointer(i);

		ProcessDataType od(tmp, data.getNumSamples() * OversamplingFactor, data.getNumChannels());

		od.copyNonAudioDataFrom(data);
		obj.process(od);

		oversampler->processSamplesDown(bl);
	}

	void initialise(NodeBase* n)
	{
		InitFunctionClass::initialise(this, n);
		obj.initialise(n);
	}

private:

	T obj;
};


template <class T> class default_data
{
	static const int NumTables = T::NumTables;
	static const int NumSliderPacks = T::NumSliderPacks;
	static const int NumAudioFiles = T::NumAudioFiles;

	default_data(T& obj)
	{
		block b;
		obj.setExternalData(b, -1);
	}

	template <typename T> void setExternalData(T& obj, const snex::ExternalData& data, int index)
	{
		obj.setExternalData(data, index);
	}
};

/** A wrapper that extends the wrap::init class with the possibility of handling external data.

	The DataHandler class needs to have a constructor with a T& argument (where you can do the 
	usual parameter initialisations). On top of that you need to:
	
	1. Define 3 static const int values: `NumTables`, `NumSliderPacks` and `NumAudioFiles`
	2. Define a `void setExternalData(T& obj, int index, const block& b)` method that distributes the
	   incoming blocks to its children.


	*/
template <class T, class DataHandler = default_data<T>> struct data : public wrap::init<T, DataHandler>
{
	static const int NumTables = DataHandler::NumTables;
	static const int NumSliderPacks = DataHandler::NumSliderPacks;
	static const int NumAudioFiles = DataHandler::NumAudioFiles;

	void setExternalData(const snex::ExternalData& data, int index)
	{
		this->i.setExternalData(obj, data, index);
	}
};

template <int BlockSize, class T> class fix_block
{
public:

	GET_SELF_OBJECT(obj.getObject());
	GET_WRAPPED_OBJECT(obj.getWrappedObject());

	fix_block() {};

	void prepare(PrepareSpecs ps)
	{
		static_functions::fix_block<BlockSize>::prepare(this, fix_block::prepareInternal, ps);
	}

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{


		static_functions::fix_block<BlockSize>::process(this, fix_block::processInternal<ProcessDataType>, data);
	}

	HISE_DEFAULT_INIT(T);
	HISE_DEFAULT_RESET(T);
	HISE_DEFAULT_MOD(T);
	HISE_DEFAULT_HANDLE_EVENT(T);

private:

	INTERNAL_PREPARE_FUNCTION(T);
	INTERNAL_PROCESS_FUNCTION(T);

	T obj;
};


/** Downsamples the incoming signal with the HISE_EVENT_RASTER value
    (default is 8x) and processes a mono signal that can be used as
    modulation signal.
*/
template <class T> class control_rate
{
public:

	SN_OPAQUE_WRAPPER(control_rate, T);

	using FrameType = snex::Types::span<float, 1>;
	using ProcessType = snex::Types::ProcessData<1>;

	constexpr static bool isModulationSource = false;

	void initialise(NodeBase* n)
	{
		obj.initialise(n);
	}

	void prepare(PrepareSpecs ps)
	{
		ps.sampleRate /= (double)HISE_EVENT_RASTER;
		ps.blockSize /= HISE_EVENT_RASTER;
		ps.numChannels = 1;

		DspHelpers::increaseBuffer(controlBuffer, ps);

		this->obj.prepare(ps);
	}

	void reset()
	{
		obj.reset();
		singleCounter = 0;
	}

	void process(ProcessType& data)
	{
		int numToProcess = data.getNumSamples() / HISE_EVENT_RASTER;

		jassert(numToProcess <= controlBuffer.size());

		FloatVectorOperations::clear(controlBuffer.begin(), numToProcess);
		
		float* d[1] = { controlBuffer.begin() };

		ProcessData<1> md(d, numToProcess, 1);
		md.copyNonAudioDataFrom(data);

		obj.process(md);
	}

	// must always be wrapped into a fix<1> node...
	void processFrame(FrameType& d)
	{
		if (--singleCounter <= 0)
		{
			singleCounter = HISE_EVENT_RASTER;
			float lastValue = 0.0f;
			obj.processFrame(d);
		}
	}

	bool handleModulation(double& )
	{
		return false;
	}

	void handleHiseEvent(HiseEvent& e)
	{
		obj.handleHiseEvent(e);
	}

	T obj;
	int singleCounter = 0;

	snex::Types::heap<float> controlBuffer;
};


/** The wrap::mod class can be used in order to create modulation connections between nodes.

	It uses the same parameter class as the containers in order to optimise as much as possible on 
	compile time.

	If you want a node to act as modulation source, you need to add a function with the prototype

	bool handleModulation(double& value)

	which sets the `value` parameter to the modulation value and returns true if the modulation is
	supposed to be sent out to the targets. The return parameter allows you to skip redundant modulation
	calls (eg. a tempo sync mod only needs to send out a value if the tempo has changed).

	The mod node will call this function on different occasions and if it returns true, forwards the value
	to the parameter class:

	- after a reset() call
	- after each process call. Be aware that processFrame will also cause a call to handle modulation so be 
	  aware of the performance implications here.
	- after each handleHiseEvent() callback. This can be used to make modulation sources that react on MIDI.

	A useful helper class for this wrapper is the ModValue class, which offers a few convenient functions.
*/
template <class ParameterClass, class T> struct mod
{
	SN_SELF_AWARE_WRAPPER(mod, T);

	template <typename ProcessDataType> void process(ProcessDataType& data) noexcept
	{
		obj.process(data);
		checkModValue();
	}

	/** Resets the node and sends a modulation signal if required. */
	void reset() noexcept 
	{
		obj.reset();
		checkModValue();
	}


	template <typename FrameDataType> void processFrame(FrameDataType& data) noexcept
	{
		obj.processFrame(data);
		checkModValue();
	}

	inline void initialise(NodeBase* n)
	{
		obj.initialise(n);
	}

	/** Calls handleHiseEvent on the wrapped object and sends out a modulation signal if required. */
	void handleHiseEvent(HiseEvent& e)
	{
		obj.handleHiseEvent(e);
		checkModValue();
	}

	bool isPolyphonic() const
	{
		return obj.isPolyphonic();
	}

	void prepare(PrepareSpecs ps)
	{
		obj.prepare(ps);
	}

	/** This function will be called when a mod value is supposed to be 
	    sent out to the target. 
	*/
	bool handleModulation(double& value) noexcept
	{
		return obj.handleModulation(value);
	}

	/** This method can be used to connect a target to the parameter of this
	    modulation node. 
	*/
	template <int I, class T> void connect(T& t)
	{
		p.getParameter<0>().connect<I>(t);
	}

	ParameterClass& getParameter() { return p; }

	/** Forwards the setParameter to the wrapped node. (Required because this
	    node needs to be *SelfAware*. 
	*/
	template <int P> void setParameter(double v)
	{
		obj.setParameter<P>(v);
	}

	T obj;
	ParameterClass p;

private:

	void checkModValue()
	{
		double modValue = 0.0;

		if (handleModulation(modValue))
			p.call(modValue);
	}
};





/** A "base class for the node template". */
struct DummyMetadata
{
	SNEX_METADATA_ID("NodeId");
	SNEX_METADATA_NUM_CHANNELS(2);
	SNEX_METADATA_PARAMETERS(3, "Value", "Reverb", "Funky");
};


template <class T, class PropertyClass = properties::none> struct node : public HiseDspBase
{
	using MetadataClass = typename T::metadata;
	static constexpr bool isModulationSource = T::isModulationSource;
	static constexpr int NumChannels = MetadataClass::NumChannels;

	// We treat everything in this node as opaque...
	SN_GET_SELF_AS_OBJECT(node);

	static Identifier getStaticId() { return MetadataClass::getStaticId(); };

	using FixBlockType = snex::Types::ProcessData<NumChannels>;
	using FrameType = snex::Types::span<float, NumChannels>;

	node() :
		obj()
	{
	}

	void initialise(NodeBase* n)
	{
		obj.initialise(n);
		props.initWithRoot(n, obj);
	}

	template <int P> static void setParameterStatic(void* ptr, double v)
	{
		auto* objPtr = &static_cast<node*>(ptr)->obj;
		T::setParameter<P>(objPtr, v);
	}

	void process(FixBlockType& d)
	{
		obj.process(d);
	}

	void process(ProcessDataDyn& data) noexcept
	{
		jassert(data.getNumChannels() == NumChannels);
		auto& fd = data.as<FixBlockType>();
		obj.process(fd);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data) noexcept
	{
		auto& fd = FrameType::as(data.begin());
		obj.processFrame(fd);
	}

	void prepare(PrepareSpecs ps)
	{
		obj.prepare(ps);
	}

	void handleHiseEvent(HiseEvent& e)
	{
		obj.handleHiseEvent(e);
	}

	bool isPolyphonic() const
	{
		return false;
	}

	void reset() noexcept { obj.reset(); }

	bool handleModulation(double& value) noexcept
	{
		return obj.handleModulation(value);
	}

	void createParameters(ParameterDataList& data)
	{
		ParameterDataList l;
		obj.parameters.addToList(l);


		jassertfalse; // Use scriptnode::ParameterEncoder and the fromT() function
#if 0
		auto pNames = MetadataClass::getParameterIds();

		for (int i = 0; i < l.size(); i++)
		{
			l.getReference(i).id = pNames[i];
		}
#endif

		data.addArray(l);
	}

	T obj;
	PropertyClass props;
};


}





}
