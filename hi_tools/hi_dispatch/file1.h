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
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once

namespace hise {
namespace dispatch {	
using namespace juce;

struct Queueable;

struct RootObject
{
	struct Child
	{
		Child(RootObject& root_);;
		virtual ~Child();

		/** return the ID of this object. */
		virtual HashedCharPtr getDispatchId() const = 0;

		RootObject& getRootObject() { return root; }
		const RootObject& getRootObject() const { return root; }
		
	private:

		RootObject& root;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Child);
	};

	explicit RootObject(PooledUIUpdater* updater_);
	~RootObject();

	void addChild(Child* c);
	void removeChild(Child* c);

	/** Removes the element from all queues. */
	int clearFromAllQueues(Queueable* q, DanglingBehaviour danglingBehaviour);

	/** Call this periodically to ensure that the queues are minimized. */
	void minimiseQueueOverhead();

	PooledUIUpdater* getUpdater();
	void setLogger(Logger* l);
	Logger* getLogger() const { return currentLogger; }

	int getNumChildren() const { return childObjects.size(); }

	void setState(State newState);
	State getState() const { return currentState; }

	/** Iterates all registered queues and calls the given function. */
	bool callForAllQueues(const std::function<bool(Queue&)>& qf);

private:

	State currentState = State::Running;

	Logger* currentLogger = nullptr;

	PooledUIUpdater* updater;
	Array<Child*> childObjects;
};

// A subclass of RootObject child that will automatically remove itself from all queues. */
struct Queueable: public RootObject::Child
{
	explicit Queueable(RootObject& r);;
	~Queueable() override;

private:

	DanglingBehaviour danglingBehaviour = DanglingBehaviour::Undefined;
};

struct Logger;

// Possible optimisation for later: use Queue for listeners too
// optional: pushes of elements with the same source and index will just overwrite the data
// TODO: add a check that assert a certain type using dynamic_cast
struct Queue final : public Queueable
{
	enum class FlushType
	{
		Flush,
		KeepData,
		numFlushTypes
	};

	static constexpr size_t MaxQueueSize = 1024 * 1024 * 4; // 4MB should be enough TODO: add dynamic upper limit with warning

	HashedCharPtr getDispatchId() const override { return HashedCharPtr("queue"); }

	struct QueuedEvent
	{
		Queueable* source = nullptr;
		uint16 numBytes = 0;
		EventType eventType = EventType::Nothing;
		//uint8 endMarker = 99;
		//uint32 u0 = 0;

		explicit operator bool() const { return source != nullptr || eventType == EventType::Nothing; }

		size_t getTotalByteSize() const;;
		static uint8* getValuePointer(uint8* ptr);
		size_t write(uint8* ptr, const void* dataValues) const;
		static QueuedEvent fromData(uint8* ptr);
	};

	/** An iterator object that walks through the data of the queue. */
	struct Iterator
	{
		explicit Iterator(Queue& queueToIterate);;
		bool next(QueuedEvent& e);

		/** Returns the position of the next element. */
		uint8* getNextPosition() const noexcept { return ptr;}

		/** Returns the position of the current element. If the iteration hasn't start, it's nullptr. */
		uint8* getPositionOfCurrentQueuable() const noexcept { return lastPos; }

		/** Rewinds the iterator and sets the last position to nullptr. */
		void rewind();

		/** Seeks the iterator to the position. */
		bool seekTo(size_t position);

	private:
		Queue& parent;
		uint8* ptr = nullptr;
		uint8* lastPos = nullptr;
	};

	void setState(State n);

	/** Packed data structure supplied to the flush function. */
	struct FlushArgument
	{
		template <typename T> T& getTypedObject() const
		{
			static_assert(std::is_base_of<Queueable, T>(), "not a base of Queueable");
			auto t = dynamic_cast<T*>(source);

			// should never be nullptr!
			jassert(t != nullptr);
			return *t;
		}

		Queueable* source = nullptr;
		uint8* data = nullptr;
		EventType eventType = EventType::Nothing;
		uint16 numBytes = 0;
	};

	using DataType = uint8;

	// The function that will be used to flush the queue. Arguments:
	// - a pointer to a Queueable
	// - a event type (used for prioritizing (TODO))
	// - the data to store
	// - the number of bytes that the data has
	using FlushFunction = std::function<bool(const FlushArgument&)>;

	/** Creates a queue and allocates the given amount of bytes. */
	Queue(RootObject& root, size_t initAllocatedSize);

	/** Checks that the allocated storage is enough for the next message.
	 *  If not, it prints a warning (TODO)
	 *  and allocates to the next power of two until the MaxQueueSize is reached
	 */
	bool ensureAllocated(size_t numBytesRequired);;

	/** Checks if the queue is empty. */
	bool isEmpty() const noexcept { return numUsed == 0; }

	/** Checks the amount of allocated bytes. */
	size_t getNumAllocated() const noexcept { return numAllocated; }

	/** pushes a event to the queue. */
	bool push(Queueable* s, EventType t, const void* values, size_t numValues);

	/** flushes the queue with the given function. If the function returns FALSE, it will abort the iteration and clean the remaining queue. */
	bool flush(const FlushFunction& f, FlushType flushType=FlushType::Flush);

	/** Attaches a logger to the queue. non-owned, lifetime of logger > queue. */
	void setLogger(Logger* l);

	/** Called by the destructor of the queuable and ensures that its removed from this queue. */
	void invalidateQueuable(Queueable* q, DanglingBehaviour behaviour);

	/** Removes the first matching value in the queue and closes the gap. Returns true if something was removed.
	 *
	 *	The offset parameter can be used to seek to the current position (if it is known).
	 */
	bool removeFirstMatchInQueue(Queueable* q, size_t offset=0);

	/** Removes all matching objects. */
	bool removeAllMatches(Queueable* q);

	void setOverrideDanglingBehaviour(DanglingBehaviour forcedBehaviour) noexcept { queueBehaviour = forcedBehaviour; }

	/** Clears the queue (just moves the pointer to the start, O(1) operation. */
	void clear() { numUsed = 0; numElements = 0; }

private:

	struct ResumeData
	{
		size_t offset = 0;
		FlushType flushType = FlushType::Flush;
		FlushFunction f;
	};
	
	State currentState = State::Running;

	ScopedPointer<ResumeData> resumeData;

	FlushArgument createFlushArgument(const QueuedEvent& e, uint8* eventPos) const noexcept;

	void clearPositionInternal(uint8* start, uint8* end);

	static uint64 alignedToPointerSize(uint64 N);
	static bool isAlignedToPointerSize(uint8* ptr);

	HeapBlock<uint8> data;
	int numUsed = 0;		// the amount of bytes used
	int numAllocated = 0;	// the amount of bytes allocated
	int numElements = 0;	// the amount of events in the queue
	DanglingBehaviour queueBehaviour = DanglingBehaviour::Undefined; // overrides the incoming behaviour request if not undefined

	Logger* attachedLogger = nullptr;
	bool logRecursion = false;
};

struct StringBuilder
{
    static constexpr int SmallBufferSize = 64;

    explicit StringBuilder(size_t numToPreallocate=0);
    
    StringBuilder& operator<<(const char* rawLiteral);
    StringBuilder& operator<<(const CharPtr& p);
    StringBuilder& operator<<(const HashedCharPtr& p);
    StringBuilder& operator<<(const String& s);
    StringBuilder& operator<<(int number);
    StringBuilder& operator<<(const StringBuilder& other);
	StringBuilder& operator<<(const Queue::FlushArgument& f);
    StringBuilder& operator<<(EventType eventType);
    StringBuilder& appendEventValues(EventType eventType, const uint8* values, size_t numBytes);

    StringBuilder& appendRawByteArray(const uint8* values, size_t numBytes)
    {
		auto& s = *this;
		s << "[ ";
		for(int i = 0; i < numBytes; i++)
		{
			s << (int)values[i];

			if(i != (numBytes-1))
				s << ", ";
		}
		s << " ] (";
        s << numBytes << " bytes)";
		return *this;
    }

    String toString() const noexcept;
    const char* get() const noexcept;
    const char* end()   const noexcept;
    size_t length() const noexcept;

private:
    
    void ensureAllocated(size_t num);
    char* getWriteHead() const;
    char* getWriteHeadAndAdvance(size_t numToWrite);
    
    ObjectStorage<SmallBufferSize, 0> data;
    int position = 0;
};

struct Logger final : public  PooledUIUpdater::SimpleTimer,
                      public Queueable
{
	Logger(RootObject& root, size_t numAllocated);;

	void flush();

	HashedCharPtr getDispatchId() const override { return "logger"; }

	void printRaw(const char* rawMessage, size_t numBytes);
	void printString(const String& message);
	void log(Queueable* source, EventType l, const void* data, size_t numBytes);
	void printQueue(Queue& queue);
    
private:

	void timerCallback() override;

    
    
	static bool logToDebugger(const Queue::FlushArgument& f);

	Queue messageQueue;
};

} // dispatch
} // hise
