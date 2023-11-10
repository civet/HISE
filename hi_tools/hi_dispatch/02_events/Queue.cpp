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



namespace hise {
namespace dispatch {	
using namespace juce;



Queueable::Queueable(RootObject& r):
RootObject::Child(r)
{
    if(auto l = r.getLogger())
    {
        auto index = r.getNumChildren();
        l->log(l, EventType::Add, (uint8*)&index, sizeof(int));
    }
}

Queueable::~Queueable()
{
    auto index = getRootObject().clearFromAllQueues(this, danglingBehaviour);
    
    if(auto l = getRootObject().getLogger())
        l->log(l, EventType::Remove, (uint8*)&index, sizeof(int));
}

size_t Queue::QueuedEvent::getTotalByteSize() const
{
    constexpr size_t headerSize = sizeof(Queueable*) + sizeof(uint16) + sizeof(EventType);
    //
    auto dataSize = headerSize + sizeof(DataType) * numBytes;
    return static_cast<size_t>(alignedToPointerSize(dataSize));
}

Queue::DataType* Queue::QueuedEvent::getValuePointer(uint8* ptr)
{
    constexpr size_t headerSize = sizeof(Queueable*) + sizeof(uint16) + sizeof(EventType);
    return ptr + headerSize; //sizeof(QueuedEvent);
}

size_t Queue::QueuedEvent::write(uint8* ptr, const void* dataValues) const
{
    jassert(isAlignedToPointerSize(ptr));
    memcpy(ptr, this, sizeof(QueuedEvent));
    memcpy(getValuePointer(ptr), dataValues, numBytes * sizeof(DataType));
    return getTotalByteSize();
}

Queue::QueuedEvent Queue::QueuedEvent::fromData(uint8* ptr)
{
    jassert(isAlignedToPointerSize(ptr));
    QueuedEvent e;
    memcpy(&e, ptr, sizeof(QueuedEvent));
    return e;
}

Queue::Iterator::Iterator(Queue& queueToIterate):
parent(queueToIterate),
ptr(parent.data.get()),
lastPos(nullptr)
{
}

bool Queue::Iterator::next(QueuedEvent& e)
{
    auto end = parent.data.get() + parent.numUsed;
    
    if(ptr < end)
    {
        e = QueuedEvent::fromData(ptr);
        lastPos = ptr;
        ptr += e.getTotalByteSize();
        return true;
    }
    
    lastPos = ptr;
    e = {};
    return false;
}

void Queue::Iterator::rewind()
{
    ptr = lastPos;
    lastPos = nullptr;
}

bool Queue::Iterator::seekTo(size_t position)
{
    if(isPositiveAndBelow(position, parent.numUsed))
    {
        ptr = parent.data.get() + position;
        return true;
    }
    else
    {
        jassertfalse;
        return false;
    }
    
}

void Queue::setState(State n)
{
    if(n == Running && resumeData != nullptr)
    {
        if(flush(resumeData->f, resumeData->flushType))
            resumeData = nullptr;
    }
}

Queue::Queue(RootObject& root, Suspendable* parent, size_t initAllocatedSize):
Suspendable(root, parent)
{
    root.addTypedChild(this);

    static_assert(sizeof(QueuedEvent) == 16, "must be 16");
    ensureAllocated(initAllocatedSize);
}

Queue::~Queue()
{
    getRootObject().removeTypedChild(this);
}

inline bool Queue::ensureAllocated(size_t numBytesRequired)
{
    if(numUsed + numBytesRequired > numAllocated)
    {
        if(attachedLogger != nullptr)
            numBytesRequired += 128; // give a bit more for the logger reallocation message...
        
        auto numToAllocate = nextPowerOfTwo(numUsed + numBytesRequired);
        
        if(isPositiveAndBelow(numToAllocate, MaxQueueSize))
        {
            HeapBlock<uint8> newData;
            newData.calloc(numToAllocate);
            memcpy(newData, data, numUsed);
            auto prev = numAllocated;
            numAllocated = numToAllocate;
            std::swap(newData, data);
            
            if(attachedLogger != nullptr)
            {
                StringBuilder b;
                b << "reallocate " << prev << " -> " << numToAllocate;
                
                QueuedEvent reallocEvent;
                reallocEvent.source = this;
                reallocEvent.eventType = EventType::LogString;
                reallocEvent.numBytes = b.length();
                numUsed += reallocEvent.write(data.get() + numUsed, b.get());
                numElements++;
            }
            
            return true;
        }
        
        return false;
    }
    
    return true;
}

bool Queue::push(Queueable* s, EventType t, const void* values, size_t numValues)
{
    // we'll only allow storing data < 256 bytes here...
    jassert(numValues < UINT8_MAX);
    jassert(!pushCheckFunction || pushCheckFunction(s));

    QueuedEvent e;
    
    e.eventType = t;
    e.numBytes = numValues;
    e.source = s;
    
    if(!ensureAllocated(e.getTotalByteSize()))
        return false;

    auto numWritten = e.write(data.get() + numUsed, values);

    jassert(!pushCheckFunction || pushCheckFunction(QueuedEvent::fromData(data.get() + numUsed).source));

	numUsed += numWritten;
    numElements++;
    return true;
}

bool Queue::flush(const FlushFunction& f, FlushType flushType)
{
    if(isEmpty() || getStateFromParent() == State::Paused)
        return true;
    
    Iterator iter(*this);
    
    if(resumeData != nullptr)
        iter.seekTo(resumeData->offset);
    
    int numDangling = 0;
    
    QueuedEvent e;
    while(iter.next(e))
    {
        if(getStateFromParent() == State::Paused)
        {
            resumeData = new ResumeData();
            resumeData->f = f;
            resumeData->flushType = flushType;
            resumeData->offset = iter.getPositionOfCurrentQueuable() - data.get();
            return true;
        }
        
        if(e.source == nullptr)
        {
            numDangling++;
            continue;
        }

        jassert(!pushCheckFunction || pushCheckFunction(e.source));

        // (eg. a change event can skip slot value changes
        auto ok = f(createFlushArgument(e, iter.getPositionOfCurrentQueuable()));
        
        if(!ok)
        {
            if(flushType == FlushType::Flush)
            {
                numUsed = 0;
                numElements = 0;
            }
            return false;
        }
    }
    
    jassert(iter.getNextPosition() == data.get() + numUsed);
    
    if(flushType == FlushType::Flush)
    {
        numUsed = 0;
        numElements = 0;
    }
    
    if(numDangling != 0 && attachedLogger)
    {
        StringBuilder m;
        m << " dangling elements after flush: " << numDangling;
        attachedLogger->log(this, EventType::Warning, m.get(), m.length());
    }
    
    return true;
}

void Queue::setLogger(Logger* l)
{
    attachedLogger = l;
}

void Queue::invalidateQueuable(Queueable* q, DanglingBehaviour behaviour)
{
    if(isEmpty())
        return;
    
    if(queueBehaviour != DanglingBehaviour::Undefined)
        behaviour = queueBehaviour;
    
    if(behaviour == DanglingBehaviour::Undefined)
        behaviour = DanglingBehaviour::Invalidate;
    
    if(behaviour == DanglingBehaviour::CloseGap)
        removeAllMatches(q);
    else
    {
        Iterator iter(*this);
        QueuedEvent e;
        
        while(iter.next(e))
        {
            if(e.source == q)
            {
                *reinterpret_cast<Queueable**>(iter.getPositionOfCurrentQueuable()) = nullptr;
            }
        }
    }
    
    
}

bool Queue::removeFirstMatchInQueue(Queueable* q, size_t offset)
{
    if(isEmpty())
        return 0;
    
    Iterator iter(*this);
    
    if(offset != 0)
        iter.seekTo(offset);
    
    QueuedEvent e;
    while(iter.next(e))
    {
        if(e.source == q)
        {
            clearPositionInternal(iter.getPositionOfCurrentQueuable(), iter.getNextPosition());
            return true;
            
        }
        else
        {
            // If you supply an offset, you will expect it to match at the position
            jassert(offset == 0);
        }
    }
    
    return false;
}

bool Queue::removeAllMatches(Queueable* q)
{
    Iterator iter(*this);
    
    bool found = false;
    
    QueuedEvent e;
    while(iter.next(e))
    {
        if(e.source == q)
        {
            found = true;
            clearPositionInternal(iter.getPositionOfCurrentQueuable(), iter.getNextPosition());
            iter.rewind();
        }
    }
    
    return found;
}

Queue::FlushArgument Queue::createFlushArgument(const QueuedEvent& e, uint8* eventPos) const noexcept
{
    FlushArgument f;
    f.source = e.source;
    f.eventType = e.eventType;
    f.numBytes = e.numBytes;
    f.data = e.getValuePointer(eventPos);
    return f;
}

void Queue::clearPositionInternal(uint8* start, uint8* end)
{
    jassert(isPositiveAndBelow(start - data.get(), numUsed));
    jassert(isPositiveAndBelow(end - data.get(), numUsed+1));
    
    auto src = end;
    auto dst = start;
    auto object_size = end - start;
    auto numToMove = (data.get() + numUsed) - end;
    memmove(dst, src, numToMove);
    numUsed -= object_size;
    numElements--;
    jassert(numElements >= 0);
    jassert(numUsed >= 0);
}

uint64 Queue::alignedToPointerSize(uint64 N)
{
    static constexpr int PointerSize = sizeof(QueuedEvent);
    
    if (N % PointerSize == 0)
        return N;
    else
        return N + (PointerSize - (N % PointerSize));
}

bool Queue::isAlignedToPointerSize(uint8* ptr)
{
    auto address = reinterpret_cast<uint64>(ptr);
    auto aligned = alignedToPointerSize(address);
    return address == aligned;
}

SomethingWithQueues::SomethingWithQueues(RootObject& r):
	Suspendable(r, nullptr),
    events(r, this, 128),
    listeners(r, nullptr, 128)
{

    listeners.forEach([](Queue& q)
    {
	    q.addPushCheck([](Queueable* s) { return dynamic_cast<Listener*>(s) != nullptr; });
    });

}

SomethingWithQueues::~SomethingWithQueues()
{
    events.forEach([](Queue& q){ q.clear(); });
    listeners.forEach([](Queue& q){ q.clear(); });
}

Queue& SomethingWithQueues::getEventQueue(DispatchType n)
{
    return events.get(n);
}

const Queue& SomethingWithQueues::getEventQueue(DispatchType n) const
{
	return events.get(n);
}

Queue& SomethingWithQueues::getListenerQueue(DispatchType n)
{
	return listeners.get(n);
}

const Queue& SomethingWithQueues::getListenerQueue(DispatchType n) const
{
	return listeners.get(n);
}

void SomethingWithQueues::processEvents(DispatchType n)
{
	if(hasEvents(n) && hasListeners(n))
	{
		StringBuilder b2;
		b2 << "process " << getDispatchId();
		TRACE_DISPATCH(DYNAMIC_STRING_BUILDER(b2));

		getEventQueue(n).flush([&](const Queue::FlushArgument& eventData)
		{
			getListenerQueue(n).flush([&](const Queue::FlushArgument& f)
			{
				auto& l = f.getTypedObject<dispatch::Listener>();
				const dispatch::Listener::EventParser p(l);
				if(const auto ld = p.parseData(f, eventData))
				{
					StringBuilder b;
					b << "process listener " << l.getDispatchId();
					TRACE_DISPATCH(DYNAMIC_STRING_BUILDER(b));
					l.slotChanged(ld);
				}
					

				return true;
			}, Queue::FlushType::KeepData);

			return true;
		});
	}
}

void SomethingWithQueues::setState(State newState)
{
	if(newState != currentState)
	{
		currentState = newState;
        events.forEach([newState](Queue& q) { q.setState(newState); });
	}
}

} // dispatch
} // hise