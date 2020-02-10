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

namespace snex {
namespace jit {
using namespace juce;
using namespace asmjit;


struct RegisterScope : public BaseScope
{
	RegisterScope(BaseScope* parentScope, const Identifier& id) :
		BaseScope({}, parentScope)
	{
		jassert(id.isValid());
		scopeId = parentScope->getScopeSymbol().getChildSymbol(id);

		jassert(getScopeType() >= BaseScope::Function);
	}

	bool hasVariable(const Identifier& id) const override;
 
	bool updateSymbol(Symbol& symbolToBeUpdated) override
	{
		jassert(getScopeForSymbol(symbolToBeUpdated) == this);

		for (auto l : localVariables)
		{
			if (l.id == symbolToBeUpdated.id)
			{
				symbolToBeUpdated = l;
				return true;
			}
		}

		return BaseScope::updateSymbol(symbolToBeUpdated);
	}

	Array<Symbol> localVariables;

private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegisterScope);
};


class FunctionScope : public RegisterScope
{
public:
	FunctionScope(BaseScope* parent, const Identifier& functionName) :
		RegisterScope(parent, functionName)
	{
		scopeType = BaseScope::Function;
	}

	~FunctionScope() {}

	bool hasVariable(const Identifier& id) const override
	{
		if (parameters.contains(id))
			return true;

		return RegisterScope::hasVariable(id);
	}

	bool updateSymbol(Symbol& symbolToBeUpdated) override
	{
		jassert(getScopeForSymbol(symbolToBeUpdated) == this);

		auto index = parameters.indexOf(symbolToBeUpdated.id);

		if (index != -1)
		{
			symbolToBeUpdated.type = data.args[index].type;
			return true;
		}

		return RegisterScope::updateSymbol(symbolToBeUpdated);
	}

	AssemblyRegister* getRegister(const Symbol& ref)
	{
		for (auto r : allocatedRegisters)
		{
			if (r->getVariableId() == ref)
				return r;
		}

		return nullptr;
	}

	void addAssemblyRegister(AssemblyRegister* newRegister)
	{
		allocatedRegisters.add(newRegister);
	}

	ReferenceCountedArray<AssemblyRegister> allocatedRegisters;

	ReferenceCountedObject* parentFunction = nullptr;
	FunctionData data;
	Array<Identifier> parameters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunctionScope);
};



}
}