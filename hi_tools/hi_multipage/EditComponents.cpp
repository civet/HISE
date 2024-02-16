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
namespace multipage {

using namespace juce;


String CodeGenerator::toString() const
{
	

	String x = getNewLine();

	x << getNewLine() << "// BEGIN AUTOGENERATED CODE";

	x << getNewLine() << "using namespace multipage;";
	x << getNewLine() << "using namespace factory;";
	x << getNewLine() << "auto mp_ = new Dialog({}, state, false);";
	x << getNewLine() << "auto& mp = *mp_;";

	for(auto& p: data[mpid::Properties].getDynamicObject()->getProperties())
	{
		x << getNewLine() << "mp.setProperty(mpid::" << p.name << ", " << p.value.toString().quoted() << ");";
	}

	if(data[mpid::Children].isArray())
	{
		int idx = 0;
		for(const auto& page: *data[mpid::Children].getArray())
		{
			x << createAddChild("mp", page, "Page", true);

			
		}
	}

	x << getNewLine() << "return mp_;";

	x << getNewLine() << "// END AUTOGENERATED CODE ";

	return x;
}

String CodeGenerator::generateRandomId(const String& prefix) const
{
	String x = prefix;

	if(x.isEmpty())
		x = "element";
	
	x << "_";
	x << String(idCounter++);
	
	return x;
}

String CodeGenerator::getNewLine() const
{
	String x = "\n";

	for(int i = 0; i < numTabs; i++)
		x << "\t";

	return x;
}

String CodeGenerator::arrayToCommaString(const var& value)
{
	String s;

	for(int i = 0; i < value.size(); i++)
	{
		s << value[i].toString();

		if(i < (value.size() - 1))
			s << ", ";
	}

	return s.quoted();
}

String CodeGenerator::createAddChild(const String& parentId, const var& childData, const String& itemType, bool attachCustomFunction) const
{
	auto id = childData[mpid::ID].toString();
	
	if(id.isEmpty())
		id = childData[mpid::Type].toString();

	id = generateRandomId(id);

	while(existingVariables.contains(id))
		id = generateRandomId(id);

	existingVariables.add(id);

	String x = getNewLine();

	auto typeId = childData[mpid::Type].toString();

	x << "auto& " << id << " = " << parentId << ".add" << itemType << "<factory::" << typeId << ">({";

	const auto& prop = childData.getDynamicObject()->getProperties();

	String cp;

	for(auto& nv: prop)
	{
		if(nv.name == mpid::Type)
			continue;

		if(nv.name == mpid::Children)
			continue;

		if(nv.value.toString().isEmpty())
			continue;

		cp << getNewLine() << "  { mpid::" << nv.name << ", ";

		if(nv.value.isString())
			cp << nv.value.toString().quoted().replace("\n", "\\n");
		else if (nv.value.isArray())
		{
			cp << arrayToCommaString(nv.value);
		}
		else
			cp << (int)(nv.value);

		cp << " }";

		cp << ", ";
	}

	x << cp.upToLastOccurrenceOf(", ", false, false);

	x << getNewLine() << "});\n";

	if(typeId == "LambdaTask")
	{
		x << getNewLine() << "// lambda task for " << id;
		x << getNewLine() << id << ".setLambdaAction(state, ";

		auto bindFunctionName = childData[mpid::Function].toString();

		if(bindFunctionName.isNotEmpty())
		{
			x << "BIND_MEMBER_FUNCTION_2(" << bindFunctionName << ")";
		}
		else
		{
			x << "[](State::Job& t, const var& stateObject)";
			x << getNewLine() << "{" << getNewLine() << "\treturn var();" << getNewLine() << "}";
		}
		
		x << ");" << getNewLine();
	}

	auto children = childData[mpid::Children];

	if(children.isArray())
	{
		for(const auto& v: *children.getArray())
		{
			x << createAddChild(id, v, "Child", false);
		}
	}

	if(attachCustomFunction)
	{
		x << getNewLine() << "// Custom callback for page " << id;
		x << getNewLine() << id << ".setCustomCheckFunction([](Dialog::PageBase* b, const var& obj)";
		x << "{\n" << getNewLine() << "\treturn Result::ok();\n" << getNewLine() << "});" << getNewLine();
	}

	return x;
}

EditorOverlay::Updater::Updater(EditorOverlay& parent_, Component* attachedComponent):
	ComponentMovementWatcher(attachedComponent),
	parent(parent_)
{
	auto root = attachedComponent->findParentComponentOfClass<Dialog>();

	root->addChildComponent(&parent);

	

	componentMovedOrResized(true, true);
	componentVisibilityChanged();

	
}

void EditorOverlay::Updater::componentMovedOrResized(bool wasMoved, bool wasResized)
{
    
    
	auto root = parent.findParentComponentOfClass<Dialog>();
	auto b = root->getLocalArea(getComponent(), parent.localBoundFunction(getComponent()));
    
    Dialog::PositionInfo position;

    if(auto laf = dynamic_cast<Dialog::LookAndFeelMethods*>(&root->getLookAndFeel()))
    {
        position = laf->getMultiPagePositionInfo(var());
    }
        
    auto fullBounds = root->getLocalBounds().reduced(position.OuterPadding);
        
    fullBounds.removeFromTop(position.TopHeight);

    

    fullBounds.removeFromTop(position.OuterPadding);
    fullBounds.removeFromBottom(position.ButtonTab);
    fullBounds.removeFromBottom(position.OuterPadding);

    b = b.getIntersection(fullBounds);
    
    parent.setBounds(b);
}

EditorOverlay::EditorOverlay(Dialog& parent)
{
	parent.addChildComponent(*this);

	localBoundFunction = [](Component* c){ return c->getLocalBounds().expanded(10); };
	
	setRepaintsOnMouseActivity(true);
}

void EditorOverlay::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel)
{
	findParentComponentOfClass<Dialog>()->getViewport().mouseWheelMove(event, wheel);
}

void EditorOverlay::resized()
{
	outline.clear();

	auto b = getLocalBounds().toFloat().reduced(3.0f);
	outline.addRoundedRectangle(b, 5.0f);

	bounds = b;

	float l[2] = { 4.0f, 4.0f };
	PathStrokeType(1.0f).createDashedStroke(outline, outline, l, 2);
}

void EditorOverlay::mouseUp(const MouseEvent& e)
{
	if(cb)
		cb(e.mods.isRightButtonDown());
}

void EditorOverlay::paint(Graphics& g)
{
	auto f = Dialog::getDefaultFont(*this).second;

	g.setColour(f.withAlpha(0.2f));
	g.fillPath(outline);

	float alpha = 0.0f;

	if(isMouseOver())
		alpha += 0.05f;

	if(isMouseButtonDown())
		alpha += 0.1f;

	if(alpha > 0.0f)
	{
		

		g.setColour(f.withAlpha(alpha));
		g.fillRoundedRectangle(bounds, 5.0f);
	}
}

} // multipage
} // hise
