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

namespace scriptnode {
using namespace juce;
using namespace hise;



bool NodeProperty::initialise(NodeBase* n)
{
	jassert(n != nullptr);

	valueTreePropertyid = baseId;

	um = n->getUndoManager();

	auto propTree = n->getPropertyTree();

	d = propTree.getChildWithProperty(PropertyIds::ID, getValueTreePropertyId().toString());

	if (!d.isValid())
	{
		d = ValueTree(PropertyIds::Property);
		d.setProperty(PropertyIds::ID, getValueTreePropertyId().toString(), nullptr);
		d.setProperty(PropertyIds::Value, defaultValue, nullptr);
		propTree.addChild(d, -1, n->getUndoManager());
	}

	postInit(n);

	return true;
}

juce::Identifier NodeProperty::getValueTreePropertyId() const
{
	return valueTreePropertyid;
}

ComboBoxWithModeProperty::ComboBoxWithModeProperty(String defaultValue, const Identifier& id):
	ComboBox(),
	mode(id, defaultValue)
{
	addListener(this);
	setLookAndFeel(&plaf);
	setColour(ColourIds::textColourId, Colour(0xFFAAAAAA));
}

void ComboBoxWithModeProperty::comboBoxChanged(ComboBox* comboBoxThatHasChanged)
{
	if (initialised)
		mode.storeValue(getText(), um);
}

void ComboBoxWithModeProperty::valueTreeCallback(Identifier id, var newValue)
{
	SafeAsyncCall::call<ComboBoxWithModeProperty>(*this, [newValue](ComboBoxWithModeProperty& c)
	{
		c.setText(newValue.toString(), dontSendNotification);
	});
}

void ComboBoxWithModeProperty::mouseDown(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DOWN(e);
	ComboBox::mouseDown(e);
}

void ComboBoxWithModeProperty::mouseDrag(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DRAG(e);
	ComboBox::mouseDrag(e);
}

void ComboBoxWithModeProperty::mouseUp(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_UP(e);
	ComboBox::mouseUp(e);
}

void ComboBoxWithModeProperty::initModes(const StringArray& modes, NodeBase* n)
{
	if (initialised)
		return;

	clear(dontSendNotification);
	addItemList(modes, 1);

	um = n->getUndoManager();
	mode.initialise(n);
	mode.setAdditionalCallback(BIND_MEMBER_FUNCTION_2(ComboBoxWithModeProperty::valueTreeCallback), true);
	initialised = true;
}
}
