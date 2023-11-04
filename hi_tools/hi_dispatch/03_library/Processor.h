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

/** The classes in this namespace will copy the existing HISE communication concept without
 *  the real logic. The actual HISE classes itself will have class members of these types
 *	which handle the actual communication.
 *
 *	The rationale behind this is that it can be incrementally changed in the HISE codebase
 *	and the modules itself (= the communication) can be tested separately.
 */
namespace library {
using namespace juce;

struct Processor;

/** This will handle the entire processor notification system.
 *  In HISE this will be a member of the MainController
 */
struct ProcessorHandler: public SourceManager
{
	enum class SlotTypes
	{
		Attributes,
		NameAndColour,
		Bypassed,
		numSlotTypes
	};

	struct BypassListener final: private dispatch::Listener
	{
		using Callback = void(*)(Processor*, bool);

		BypassListener(RootObject& r, const Callback& f);;
		~BypassListener() override {};
		
	private:

		friend class Processor;

		Callback f;
		void slotChanged(const ListenerData& d) override;
	};

	struct AttributeListener final: private dispatch::Listener
	{
		using Callback = void(*)(Processor*, uint8);

		AttributeListener(RootObject& r, const Callback& f_);;
		~AttributeListener() final {};
		
	private:

		friend class Processor;
		const Callback f;
		
		void slotChanged(const ListenerData& d) final;
	};

	ProcessorHandler(RootObject& r);
};


/** The communication handler for the hise::Processor base class. It handles changes
 *  to attributes, bypass state and name / ID changes in separate slots
 */
struct Processor: public Source
{
	using SlotTypes = ProcessorHandler::SlotTypes;
	using BypassListener = ProcessorHandler::BypassListener;
	using AttributeListener = ProcessorHandler::AttributeListener;

	Processor(ProcessorHandler& h, const HashedCharPtr& id);;
	~Processor() override;

	/** Call this whenever the attributes change. You can specify a notification type
	 *
	 *	The notification will always be send out synchronously.
	 */
	void setAttribute(int parameterIndex, float, NotificationType n);

	/** Call this in the processor if the bypass state changes. This will cache the value
	 *  so that the BypassListener API is consistent with the current HISE API.
	 */
	void setBypassed(bool value);

	/** Call this in the processor if the ID changes. */
	void setId(const String& );

	/** Call this in the processor if the colour changes. */
	void setColour(const Colour& );

	/** Call this in the processor if the amount of attributes change. */
	void setNumAttributes(int numAttributes);

	/** Adds a Bypass listener to receive notifications. */
	void addBypassListener(BypassListener* l, NotificationType n);

	/** Removes a bypass listener from all listener queues (both sync and async). */
	void removeBypassListener(BypassListener* l);

	void addAttributeListener(AttributeListener* l, const uint8* attributeIndexes, size_t numAttributes, NotificationType n);

	bool isBypassed() const noexcept;

private:

	bool cachedBypassValue = false;

	SlotSender attributes;
	SlotSender nameAndColour;
	SlotSender bypassed;
};

struct Modulator final: public Processor
{
	enum class ModulatorSlotTypes
	{
		Intensity,
		Other,		// monophonic / retrigger, etc
		numModulatorSlotTypes
	};

	Modulator(ProcessorHandler& h, const HashedCharPtr& id);;
	~Modulator() override;

	SlotSender intensity;
	SlotSender other;
};

} // library
} // dispatch
} // hise