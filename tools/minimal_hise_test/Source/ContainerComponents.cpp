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
namespace factory {
using namespace juce;

Container::Container(Dialog& r, int width, const var& obj):
	PageBase(r, width, obj)
{
    if(obj.hasProperty(mpid::ID))
    {
	    id = obj[mpid::ID].toString();
    }

	auto l = obj["Children"];
        
	if(l.isArray())
	{
		for(auto& r: *l.getArray())
			addChild(width, r);
	}
}

void Container::postInit()
{
    init();

    stateObject = Dialog::getOrCreateChild(stateObject, id);

	for(const auto& sp: staticPages)
	{
		childItems.add(sp->create(rootDialog, getWidth()));
		addAndMakeVisible(childItems.getLast());
	}

	for(auto c: childItems)
    {
        c->setStateObject(stateObject);
        c->postInit();
    }

	calculateSize();
}

Result Container::checkGlobalState(var globalState)
{
    var toUse = globalState;

    if(id.isValid())
    {
	    if(toUse.hasProperty(id))
            toUse = toUse[id];
        else
        {
            auto no = new DynamicObject();
	        toUse.getDynamicObject()->setProperty(id, no);
            toUse = var(no);
        }
    }

	for(auto c: childItems)
	{
		auto ok = c->check(toUse);
            
		if(!ok.wasOk())
			return ok;
	}
        
	return Result::ok();
}

void Container::addChild(Dialog::PageInfo::Ptr info)
{
	staticPages.add(info);
}

void Container::addChild(int width, const var& r)
{
	if(auto pi = factory.create(r))
	{
		childItems.add(pi->create(rootDialog, width));
		addAndMakeVisible(childItems.getLast());
	}
}

void List::calculateSize()
{
	int h = foldable ? (titleHeight + padding) : 0;

    for(auto c: childItems)
        c->setVisible(!folded);
    
    if(!folded)
    {
        for(auto& c: childItems)
            h += c->getHeight() + padding;
    }
    
    

	setSize(getWidth(), h);
}

List::List(Dialog& r, int width, const var& obj):
	Container(r, width, obj)
{
    foldable = obj[mpid::Foldable];
    folded = obj[mpid::Folded];
    title = obj[mpid::Text];
	setSize(width, 0);
}

void List::createEditorInfo(Dialog::PageInfo* info)
{
    auto& xxx = *info;

    auto& tt = xxx.addChild<Type>({
        { mpid::Type, "List" },
        { mpid::ID, "Type"}
    });
    
    auto& prop = xxx.addChild<List>();
    
    xxx[mpid::Text] = "List";
    
    prop[mpid::Folded] = false;
    prop[mpid::Foldable] = true;
    prop[mpid::Padding] = 10;
    prop[mpid::Text] = "Properties";
    
    auto& listId = prop.addChild<TextInput>({
        { mpid::ID, "ID" },
        { mpid::Text, "ID" },
        { mpid::Required, true},
        { mpid::Help, "The ID of the list. This will be used for identification in some logic cases" }
    });

    if(!xxx[mpid::Value].isUndefined())
    {
        auto v = xxx[mpid::Value].toString();

	    listId[mpid::Value] = v;
    }

    auto& textId = prop.addChild<TextInput>({
        { mpid::ID, "Text" },
        { mpid::Text, "Text" },
        { mpid::Help, "The title text that is shown at the header bar." }
    });

    auto& padId = prop.addChild<TextInput>({
        { mpid::ID, "Padding" },
        { mpid::Text, "Padding" },
        { mpid::Help, "The spacing between child elements in pixel." }
    });

    auto& foldId1 = prop.addChild<Tickbox>({
        { mpid::ID, "Foldable" },
        { mpid::Text, "Foldable" },
        { mpid::Help, "If ticked, then this list will show a clickable header that can be folded" }
    });
    
    auto& foldId2 = prop.addChild<Tickbox>({
        { mpid::ID, "Folded" },
        { mpid::Text, "Folded" },
        { mpid::Help, "If ticked, then this list will folded as default state" }
    });
    
    auto& ff = xxx.addChild<Builder>({
        { mpid::ID, "Children" },
        { mpid::Text, "Children" }
    });
    
    auto& typeOptions = ff.addChild<Branch>();
    
    Dialog::Factory f;
    
    bool addSelf = false;
    
    for(auto type: f.getIdList())
    {
        if(type == "List")
        {
            addSelf = true;
        }
        else
        {
            auto& itemBuilder =  typeOptions.addChild<List>();
            itemBuilder[mpid::Text] = type;

            auto obj = new DynamicObject();
            obj->setProperty(mpid::ID, type + "Id");
            obj->setProperty(mpid::Type, type);
            
            Dialog::PageInfo::Ptr c = f.create(obj);
            ScopedPointer<Dialog::PageBase> c2 = c->create(rootDialog, 0);
            c2->createEditorInfo(&itemBuilder);
        }
    }
    
    if(addSelf)
        typeOptions.childItems.add(info->clone());
}

void List::resized()
{
	auto b = getLocalBounds();

	if(b.isEmpty())
		return;

    if(foldable)
        b.removeFromTop(24 + padding);
    
    if(!folded)
    {
        for(auto c: childItems)
        {
            c->setBounds(b.removeFromTop(c->getHeight()));
            b.removeFromTop(padding);
        }
    }
}

void List::mouseDown(const MouseEvent& e)
{
	if(foldable && e.getPosition().getY() < titleHeight)
	{
		folded = !folded;
            
		Container* c = this;
            
		c->calculateSize();
		repaint();
            
		while(c = dynamic_cast<Container*>(c->getParentComponent()))
		{
			c->calculateSize();
		}
	}
}

void List::paint(Graphics& g)
{
	if(foldable)
	{
		if(auto laf = dynamic_cast<Dialog::LookAndFeelMethods*>(&rootDialog.getLookAndFeel()))
		{
			auto b = getLocalBounds().removeFromTop(titleHeight).toFloat();
			laf->drawMultiPageFoldHeader(g, *this, b, title, folded);
		}
	}
}

Column::Column(Dialog& r, int width, const var& obj):
	Container(r, width, obj)
{
	padding = (int)obj[mpid::Padding];

	
    auto widthList = obj[mpid::Width];
    
    if(childItems.size() > 0)
    {
        auto equidistance = -1.0 / childItems.size();
        
        for(int i = 0; i < childItems.size(); i++)
        {
            auto v = widthList[i];
            
            if(v.isUndefined() || v.isVoid())
                widthInfo.add(equidistance);
            else
                widthInfo.add((double)v);
        }
    }

	setSize(width, 0);
}

void Column::calculateSize()
{
	int h = 0;

	for(auto& c: childItems)
		h = jmax(h, c->getHeight());
	        
	setSize(getWidth(), h);
}

void Column::resized()
{
	auto b = getLocalBounds();

	if(b.isEmpty())
		return;

	auto fullWidth = getWidth();
        
	for(const auto& w: widthInfo)
	{
		if(w > 0.0)
			fullWidth -= (int)w;

		fullWidth -= padding;
	}
	
	for(int i = 0; i < childItems.size(); i++)
	{
		auto w = widthInfo[i];

		if(w < 0.0)
			w = fullWidth * (-1.0) * w;

		childItems[i]->setBounds(b.removeFromLeft(roundToInt(w)));
		b.removeFromLeft(padding);
	}
}

void Column::postInit()
{
	if(!staticPages.isEmpty())
	{
		auto equidistance = -1.0 / staticPages.size();
            
		for(auto sp: staticPages)
		{
			auto v = (*sp)[mpid::Width];
                
			if(v.isUndefined() || v.isVoid())
				widthInfo.add(equidistance);
			else
				widthInfo.add((double)v);
		}
	}
	    
	Container::postInit();
}

Branch::Branch(Dialog& root, int w, const var& obj):
	Container(root, w, obj)
{
	setSize(w, 0);
}

Dialog::PageBase* Branch::createFromStateObject(const var& obj, int w)
{
	for(auto& sp: staticPages)
	{
		if(sp->data[mpid::Text].toString() == obj[mpid::Type].toString())
			return sp->create(rootDialog, w);
	}

	return nullptr;
}

Dialog::PageBase* Branch::createWithPopup(int width)
{
	PopupLookAndFeel plaf;
	PopupMenu m;
	m.setLookAndFeel(&plaf);
        
	int index = 0;
        
	for(auto sp: staticPages)
	{
		m.addItem(index + 1, sp->getData()[mpid::Text].toString());
		index++;
	}
        
	auto result = m.show();
        
	if(isPositiveAndBelow(result-1, staticPages.size()))
	{
		if(auto sp = staticPages[result - 1])
			return sp->create(rootDialog, width);
	}
        
	return nullptr;
}

void Branch::postInit()
{
	currentIndex = getValueFromGlobalState();
        
	for(const auto& sp: staticPages)
	{
		childItems.add(sp->create(rootDialog, getWidth()));
		addAndMakeVisible(childItems.getLast());
	}
            
	if(PageBase* p = childItems.removeAndReturn(currentIndex))
	{
		childItems.clear();
		childItems.add(p);
		p->postInit();
	}
	else
		childItems.clear();
        
	calculateSize();
}

void Branch::calculateSize()
{
	if(auto c = childItems[0])
	{
		c->setVisible(true);
		setSize(getWidth(), c->getHeight());
	}
}

Result Branch::checkGlobalState(var globalState)
{
	if(auto p = childItems[0])
		return p->check(globalState);
        
	return Result::fail("No branch selected");
}

void Branch::resized()
{
	if(auto p = childItems[0])
		p->setBounds(getLocalBounds());
}

Builder::Builder(Dialog& r, int w, const var& obj):
	Container(r, w, obj),
	addButton("add", nullptr, r)
{
	title = obj[mpid::Text];
	padding = jmax(padding, 10);
        
	childItems.clear();
	addAndMakeVisible(addButton);
	addButton.onClick = BIND_MEMBER_FUNCTION_0(Builder::onAddButton);
	setSize(w, 32);
}

void Builder::mouseDown(const MouseEvent& e)
{
	if(e.getPosition().getY() < 32)
	{
		folded = !folded;
		rebuildPosition();
		repaint();
	}
}

void Builder::calculateSize()
{
	int h = 32 + padding;
        
	for(auto& c: childItems)
	{
		c->setVisible(!folded);
            
		if(!folded)
			h += c->getHeight() + 3 * padding;
	}
        
	setSize(getWidth(), h);
}

void Builder::resized()
{
	auto b = getLocalBounds();
	addButton.setBounds(b.removeFromTop(32).removeFromRight(32).withSizeKeepingCentre(24, 24));
	b.removeFromTop(padding);
	itemBoxes.clear();
        
	for(int i = 0; i < childItems.size(); i++)
	{
		auto c = childItems[i];
		auto row = b.removeFromTop(2 * padding + c->getHeight());
		itemBoxes.addWithoutMerging(row);
		auto bb = row.removeFromRight(32).removeFromTop(32).withSizeKeepingCentre(20, 20);
		row = row.reduced(padding);
		closeButtons[i]->setVisible(c->isVisible());
            
		if(!folded)
		{   
			closeButtons[i]->setBounds(bb);
			c->setBounds(row);
			b.removeFromTop(padding);
		}
	}
}

void Builder::paint(Graphics& g)
{
	for(auto& b: itemBoxes)
	{
		g.setColour(Colours::white.withAlpha(0.2f));
		g.drawRoundedRectangle(b.toFloat().reduced(1.0f), 5.0f, 2.0f);
	}
        
	if(auto laf = dynamic_cast<Dialog::LookAndFeelMethods*>(&rootDialog.getLookAndFeel()))
	{
		auto b = getLocalBounds().removeFromTop(32).toFloat();
		laf->drawMultiPageFoldHeader(g, *this, b, title, folded);
	}
}

Result Builder::checkGlobalState(var globalState)
{
	int idx = 0;
        
	for(auto& c: childItems)
	{
		var obj;
            
		if(isPositiveAndBelow(idx, stateList.size()))
			obj = stateList[idx];
            
		if(obj.getDynamicObject() == nullptr)
		{
			stateList.insert(idx, var(new DynamicObject()));
			obj = stateList[idx];
		}
            
		auto ok = c->check(obj);
            
		if(ok.failed())
			return ok;
            
		idx++;
	}
        
	DBG(JSON::toString(stateList));
        
	return Result::ok();
}

void Builder::addChildItem(PageBase* b, const var& stateInArray)
{
	childItems.add(b);
	closeButtons.add(new HiseShapeButton("close", nullptr, rootDialog));
	addAndMakeVisible(closeButtons.getLast());
	addAndMakeVisible(childItems.getLast());
	closeButtons.getLast()->addListener(this);
	childItems.getLast()->setStateObject(stateInArray);
	childItems.getLast()->postInit();
}

void Builder::createItem(const var& stateInArray)
{
	for(const auto& sp: staticPages)
	{
		auto b = sp->create(rootDialog, getWidth());
		addChildItem(b, stateInArray);
	}
}

void Builder::onAddButton()
{
	var no(new DynamicObject());
	stateList.insert(-1, no);
	folded = false;
        
	if(popupOptions != nullptr)
	{
		if(auto nb = popupOptions->createWithPopup(getWidth()))
			addChildItem(nb, no);
	}
	else
		createItem(no);
        
	rebuildPosition();
}

void Builder::rebuildPosition()
{
	auto t = dynamic_cast<Container*>(this);
        
	while(t != nullptr)
	{
		t->calculateSize();
		t = t->findParentComponentOfClass<Container>();
	}
	repaint();
}

void Builder::buttonClicked(Button* b)
{
	auto cb = dynamic_cast<HiseShapeButton*>(b);
	auto indexToDelete = closeButtons.indexOf(cb);
        
	stateList.getArray()->remove(indexToDelete);
	closeButtons.removeObject(cb);
	childItems.remove(indexToDelete);
        
	rebuildPosition();
}

void Builder::postInit()
{
	stateList = getValueFromGlobalState();
        
	if(auto firstItem = staticPages.getFirst())
	{
		ScopedPointer<PageBase> fi = firstItem->create(rootDialog, getWidth());
            
		if(auto br = dynamic_cast<Branch*>(fi.get()))
		{
			popupOptions = br;
			fi.release();
		}
	}
        
	if(!stateList.isArray())
	{
		stateList = var(Array<var>());
		writeState(stateList);
	}
	else
	{
		for(auto& obj: *stateList.getArray())
		{
			if(popupOptions != nullptr)
			{
				if(auto nb = popupOptions->createFromStateObject(obj, getWidth()))
				{
					addChildItem(nb, obj);
				}
			}
			else
			{
				createItem(obj);
			}
		}
	}

	calculateSize();
}
} // PageFactory
} // multipage
} // hise