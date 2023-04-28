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





namespace snex {
namespace mir {
using namespace juce;

MirBuilder::MirBuilder(MIR_context* ctx_, const ValueTree& v_) :
	root(v_)
{
	currentState = new State();
	currentState->ctx = ctx_;

	REGISTER_TYPE(SyntaxTree);
	REGISTER_TYPE(Function);
	REGISTER_TYPE(ReturnStatement);
	REGISTER_TYPE(BinaryOp);
	REGISTER_TYPE(VariableReference);
	REGISTER_TYPE(Immediate);
	REGISTER_TYPE(Cast);
	REGISTER_TYPE(Assignment);
	REGISTER_TYPE(Comparison);
	REGISTER_TYPE(TernaryOp);
	REGISTER_TYPE(IfStatement);
	REGISTER_TYPE(LogicalNot);
	REGISTER_TYPE(WhileLoop);
	REGISTER_TYPE(Increment);
	REGISTER_TYPE(StatementBlock);
	REGISTER_TYPE(ComplexTypeDefinition);
	REGISTER_TYPE(Subscript);
	REGISTER_TYPE(InternalProperty);
	REGISTER_TYPE(ControlFlowStatement);
    REGISTER_TYPE(Loop);
	REGISTER_TYPE(FunctionCall);
	REGISTER_TYPE(ClassStatement);
	REGISTER_TYPE(Dot);
	REGISTER_TYPE(ThisPointer);
	REGISTER_TYPE(PointerAccess);
    
    REGISTER_INLINER(dyn__referTo_ppii);
    REGISTER_INLINER(dyn__size_i);
}

MirBuilder::~MirBuilder()
{
	delete currentState;
}

juce::Result MirBuilder::parse()
{
	return parseInternal(root);
}

MIR_module* MirBuilder::getModule() const
{
	return currentState->currentModule;
}

void MirBuilder::setDataLayout(const String& b64)
{
    currentState->setDataLayout(b64);
}

String MirBuilder::getMirText() const
{
	auto text = currentState->toString(true);
	DBG(text);
	return text;
}

juce::Result MirBuilder::parseInternal(const ValueTree& v)
{
	return currentState->processTreeElement(v);
}

bool MirBuilder::checkAndFinish(const Result& r)
{
	if (r.failed())
	{
		return true;
	}

	return r.failed();
}

}
}