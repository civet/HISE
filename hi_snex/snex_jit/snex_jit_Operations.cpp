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
namespace jit {
using namespace juce;
using namespace asmjit;



void Operations::Function::process(BaseCompiler* compiler, BaseScope* scope)
{
	Statement::process(compiler, scope);

	COMPILER_PASS(BaseCompiler::FunctionParsing)
	{
		functionScope = new FunctionScope(scope, id.id);

		for (int i = 0; i < data.args.size(); i++)
		{
			data.args.getReference(i).id = parameters[i];
		}


		functionScope->data = data;

		functionScope->parameters.addArray(parameters);
		functionScope->parentFunction = dynamic_cast<ReferenceCountedObject*>(this);

		auto classData = new FunctionData(data);

		dynamic_cast<ClassScope*>(scope)->addFunction(classData);

		try
		{
			FunctionParser p(compiler, id, *this);

			BlockParser::ScopedScopeStatementSetter svs(&p, findParentStatementOfType<ScopeStatementBase>(this));

			p.currentScope = functionScope;
			statements = p.parseStatementList();
			
			compiler->executePass(BaseCompiler::PreSymbolOptimization, functionScope, statements);
			// add this when using stack...
			//compiler->executePass(BaseCompiler::DataSizeCalculation, functionScope, statements);
			compiler->executePass(BaseCompiler::DataAllocation, functionScope, statements);
			compiler->executePass(BaseCompiler::DataInitialisation, functionScope, statements);
			
			compiler->executePass(BaseCompiler::ResolvingSymbols, functionScope, statements);
			compiler->executePass(BaseCompiler::TypeCheck, functionScope, statements);
			compiler->executePass(BaseCompiler::SyntaxSugarReplacements, functionScope, statements);
			compiler->executePass(BaseCompiler::PostSymbolOptimization, functionScope, statements);

			compiler->setCurrentPass(BaseCompiler::FunctionParsing);
		}
		catch (ParserHelpers::CodeLocation::Error& e)
		{
			statements = nullptr;
			functionScope = nullptr;

			throw e;
		}
	}

	COMPILER_PASS(BaseCompiler::FunctionCompilation)
	{
		ScopedPointer<asmjit::StringLogger> l = new asmjit::StringLogger();

		auto runtime = getRuntime(compiler);

		ScopedPointer<asmjit::CodeHolder> ch = new asmjit::CodeHolder();
		ch->setLogger(l);
		ch->setErrorHandler(this);
		ch->init(runtime->codeInfo());
		
		//code->setErrorHandler(this);

		ScopedPointer<asmjit::X86Compiler> cc = new asmjit::X86Compiler(ch);

		FuncSignatureX sig;

		hasObjectPtr = scope->getParent()->getScopeType() == BaseScope::Class;

		AsmCodeGenerator::fillSignature(data, sig, hasObjectPtr);
		cc->addFunc(sig);

		dynamic_cast<ClassCompiler*>(compiler)->setFunctionCompiler(cc);

		compiler->registerPool.clear();

		if (hasObjectPtr)
		{
			objectPtr = compiler->registerPool.getNextFreeRegister(functionScope, Types::ID::Pointer);

			auto asg = CREATE_ASM_COMPILER(Types::ID::Pointer);
			asg.emitParameter(this, objectPtr, -1);
		}

		compiler->executePass(BaseCompiler::PreCodeGenerationOptimization, functionScope, statements);
		compiler->executePass(BaseCompiler::RegisterAllocation, functionScope, statements);
		compiler->executePass(BaseCompiler::CodeGeneration, functionScope, statements);

		cc->endFunc();
		cc->finalize();
		cc = nullptr;

		runtime->add(&data.function, ch);

        jassert(data.function != nullptr);
        
		auto fClass = dynamic_cast<FunctionClass*>(scope);

		bool success = fClass->injectFunctionPointer(data);

		ignoreUnused(success);
		jassert(success);

		auto& as = dynamic_cast<ClassCompiler*>(compiler)->assembly;

		as << "; function " << data.getSignature() << "\n";
		as << l->data();

		ch->setLogger(nullptr);
		l = nullptr;
		ch = nullptr;

		compiler->setCurrentPass(BaseCompiler::FunctionCompilation);
	}
}

void Operations::SmoothedVariableDefinition::process(BaseCompiler* compiler, BaseScope* scope)
{
	Statement::process(compiler, scope);

	COMPILER_PASS(BaseCompiler::ResolvingSymbols)
	{
		jassert(scope->getScopeType() == BaseScope::Class);

		if (auto cs = dynamic_cast<ClassScope*>(scope))
		{
			if (type == Types::ID::Float)
				cs->addFunctionClass(new SmoothedFloat<float>(id, iv.toFloat()));
			else if (type == Types::ID::Double)
				cs->addFunctionClass(new SmoothedFloat<double>(id, iv.toDouble()));
			else
				throwError("Wrong type for smoothed value");
		}

	}

	COMPILER_PASS(BaseCompiler::TypeCheck)
	{
		if (type != iv.getType())
			logWarning("Type mismatch at smoothed value");
	}
}

#if 0
void Operations::WrappedBlockDefinition::process(BaseCompiler* compiler, BaseScope* scope)
{
	COMPILER_PASS(BaseCompiler::ResolvingSymbols)
	{
		jassert(scope->getScopeType() == BaseScope::Class);

		if (auto cs = dynamic_cast<ClassScope*>(scope))
		{
			cs->addFunctionClass(new WrappedBuffer<BufferHandler::WrapAccessor<>>(id, b));
		}
	}
}
#endif

Operations::TokenType Operations::VariableReference::getWriteAccessType()
{
	if (auto as = dynamic_cast<Assignment*>(parent.get()))
	{
		if (as->getSubExpr(1).get() == this)
			return as->assignmentType;
	}
	else if (auto inc = dynamic_cast<Operations::Increment*>(parent.get()))
		return inc->isDecrement ? JitTokens::minus : JitTokens::plus;

	return JitTokens::void_;
}

void Operations::VariableReference::process(BaseCompiler* compiler, BaseScope* scope)
{
	Expression::process(compiler, scope);

	COMPILER_PASS(BaseCompiler::DataAllocation)
	{
		if (variableScope != nullptr)
			return;

		if (auto f = getFunctionClassForSymbol(scope))
		{
			id.typeInfo = TypeInfo(Types::ID::Pointer, true);
			return;
		}

		// walk up the dot operators to get the proper symbol...
		if (auto dp = dynamic_cast<DotOperator*>(parent.get()))
		{
			if (auto pPointer = dp->getDotParent()->getComplexType())
			{
				if (auto sType = dynamic_cast<StructType*>(pPointer.get()))
				{
					objectPointer = dp->getDotParent();

					id.typeInfo = sType->getMemberTypeInfo(id.id);
					auto byteSize = id.typeInfo.getRequiredByteSize();

					if (byteSize == 0)
						location.throwError("Can't deduce type size");

					memberOffset = VariableStorage((int)(sType->getMemberOffset(id.id)));
					return;
				}
			}
		}

		if (id.isReference() || isLocalDefinition)
		{
			variableScope = scope;

			if (!variableScope->updateSymbol(id))
				location.throwError("Can't update symbol" + id.toString());
		}
		else if (auto vScope = scope->getScopeForSymbol(id))
		{
			if (!vScope->updateSymbol(id))
				location.throwError("Can't update symbol " + id.toString());

			if (auto fScope = dynamic_cast<FunctionScope*>(vScope))
				parameterIndex = fScope->parameters.indexOf(id.id);

			variableScope = vScope;
		}
		else if (auto typePtr = scope->getRootData()->getComplexTypeForVariable(id))
		{
			dataPointer = VariableStorage(Types::ID::Pointer, scope->getRootData()->getDataPointer(id), true);

			id.typeInfo = TypeInfo(typePtr, id.isConst());
			id.typeInfo.setType(Types::ID::Pointer);
		}
		else
			location.throwError("Can't resolve symbol " + id.toString());

		if (auto cScope = dynamic_cast<ClassScope*>(variableScope.get()))
		{
			if (getType() == Types::ID::Dynamic)
				location.throwError("Use of undefined variable " + id.toString());

			if (auto subClassType = dynamic_cast<StructType*>(cScope->typePtr.get()))
				memberOffset = VariableStorage((int)subClassType->getMemberOffset(id.id));
		}
	}

	COMPILER_PASS(BaseCompiler::TypeCheck)
	{
		if (objectPointer != nullptr && objectPointer->getType() != Types::ID::Pointer)
			objectPointer->location.throwError("expression must have class type");
	}

	COMPILER_PASS(BaseCompiler::RegisterAllocation)
	{
		if (isConstExpr())
		{
			Ptr c = new Immediate(location, getConstExprValue());
			replaceInParent(c);
			return;
		}

		// We need to initialise parameter registers before the rest
		if (parameterIndex != -1)
		{
			reg = compiler->registerPool.getRegisterForVariable(scope, id);

			if (isFirstReference())
			{
				if (auto fScope = scope->getParentScopeOfType<FunctionScope>())
				{
					auto asg = CREATE_ASM_COMPILER(getType());
					asg.emitParameter(dynamic_cast<Function*>(fScope->parentFunction), reg, parameterIndex);
				}
			}

			return;
		}
	}

	COMPILER_PASS(BaseCompiler::CodeGeneration)
	{
		if (BlockAccess::isWrappedBufferReference(this, scope))
			return;

		if (parameterIndex != -1)
			return;

		if (isApiClass(scope))
			return;

		// It might already be assigned to a reused register
		if (reg == nullptr)
		{
			if (objectPointer == nullptr && memberOffset.getType() != Types::ID::Void)
			{
				if (auto fs = scope->getParentScopeOfType<FunctionScope>())
				{
					if (auto pf = dynamic_cast<Function*>(fs->parentFunction))
					{
						reg = compiler->registerPool.getNextFreeRegister(scope, getType());
						reg->setReference(scope, id);
						auto acg = CREATE_ASM_COMPILER(getType());
						acg.emitThisMemberAccess(reg, pf->objectPtr, memberOffset);
						return;
					}
				}
			}

			if (!dataPointer.isVoid())
			{
				auto asg = CREATE_ASM_COMPILER(Types::ID::Pointer);
				reg = compiler->registerPool.getNextFreeRegister(scope, Types::ID::Pointer);
				reg->setDataPointer(dataPointer.getDataPointer());
				reg->createMemoryLocation(asg.cc);
				return;
			}
			if (objectPointer != nullptr)
			{
				// in this case we'll just store the offset as an integer register.
				auto asg = CREATE_ASM_COMPILER(Types::ID::Pointer);
				reg = compiler->registerPool.getNextFreeRegister(scope, Types::ID::Integer);
				reg->setDataPointer(memberOffset.getDataPointer());
				reg->createMemoryLocation(asg.cc);
				return;
			}
			else
			{
				jassert(variableScope != nullptr);
				reg = compiler->registerPool.getRegisterForVariable(variableScope, id);
			}
		}
			
		if (reg->isActiveOrDirtyGlobalRegister() && findParentStatementOfType<ConditionalBranch>(this) != nullptr)
		{
			// the code generation has already happened before the branch so that we have the global register
			// available in any case
			return;
		}

		if (reg->isIteratorRegister())
			return;

		auto asg = CREATE_ASM_COMPILER(type);

		isFirstOccurence = (!reg->isActiveOrDirtyGlobalRegister() && !reg->isMemoryLocation()) || isFirstReference();

		if (isFirstOccurence)
		{
			auto assignmentType = getWriteAccessType();

			auto rd = scope->getRootClassScope()->rootData.get();

			if (auto cScope = dynamic_cast<ClassScope*>(scope->getScopeForSymbol(id)))
			{
				if (cScope->typePtr != nullptr)
				{
					if (auto fScope = scope->getParentScopeOfType<FunctionScope>())
					{
						auto ptr = dynamic_cast<Function*>(fScope->parentFunction)->objectPtr;
						jassertfalse;

						return;
					}
				}
			}

			if (variableScope->getScopeType() == BaseScope::Class && rd->contains(id))
			{
				auto dataPointer = rd->getDataPointer(id);

				if (assignmentType != JitTokens::void_)
				{
					if (assignmentType != JitTokens::assign_)
					{
						reg->setDataPointer(dataPointer);
						reg->loadMemoryIntoRegister(asg.cc);
					}
					else
						reg->createRegister(asg.cc);
				}
				else
				{
					reg->setDataPointer(dataPointer);
					reg->createMemoryLocation(asg.cc);

					if (!isReferencedOnce())
						reg->loadMemoryIntoRegister(asg.cc);
				}
			}
		}
	}
}

bool Operations::VariableReference::isApiClass(BaseScope* s) const
{
	if (auto fc = getFunctionClassForSymbol(s))
	{
		return dynamic_cast<ClassScope*>(fc) == nullptr;
	}
    
    return false;
}

void Operations::Assignment::process(BaseCompiler* compiler, BaseScope* scope)
{
	/*
	ResolvingSymbols: check that target is not const
	TypeCheck, = > check type match
	DeadCodeElimination, = > remove unreferenced local variables
	Inlining, = > make self assignment
	CodeGeneration, = > Store or Op
	*/

	

	/** Assignments might use the target register OR have the same symbol from another scope
	    so we need to customize the execution order in these passes... */
	if (compiler->getCurrentPass() == BaseCompiler::CodeGeneration ||
		compiler->getCurrentPass() == BaseCompiler::DataAllocation)
	{
		Statement::process(compiler, scope);
	}
	else
	{
		Expression::process(compiler, scope);
	}

	auto e = getSubExpr(0);



	COMPILER_PASS(BaseCompiler::DataSizeCalculation)
	{
		if (targetType == TargetType::Variable && isFirstAssignment && scope->getRootClassScope() == scope)
		{
			auto typeToAllocate = getTargetVariable()->getType();

			if (typeToAllocate == Types::ID::Dynamic)
			{
				int x = 12;

				typeToAllocate = getSubExpr(0)->getType();

				if (!Types::Helpers::isFixedType(typeToAllocate))
				{
					location.throwError("Can't deduce type");

				}

				getTargetVariable()->id.typeInfo.setType(typeToAllocate);
			}

			scope->getRootData()->enlargeAllocatedSize(Types::Helpers::getSizeForType(typeToAllocate));
		}
	}

	COMPILER_PASS(BaseCompiler::DataAllocation)
	{
		getSubExpr(0)->process(compiler, scope);

		if ((targetType == TargetType::Variable || targetType == TargetType::Reference) && isFirstAssignment)
		{
			auto type = getTargetVariable()->getType();

			if (!Types::Helpers::isFixedType(type))
			{
				type = getSubExpr(0)->getType();

				if (!Types::Helpers::isFixedType(type))
				{
					BaseCompiler::ScopedPassSwitcher rs(compiler, BaseCompiler::ResolvingSymbols);
					getSubExpr(0)->process(compiler, scope);

					BaseCompiler::ScopedPassSwitcher tc(compiler, BaseCompiler::TypeCheck);
					getSubExpr(0)->process(compiler, scope);

					type = getSubExpr(0)->getType();

					if (!Types::Helpers::isFixedType(type))
						location.throwError("Can't deduce auto type");
				}

				getTargetVariable()->id.typeInfo.setType(type);
			}

			getTargetVariable()->isLocalDefinition = true;
			scope->addVariable(getTargetVariable()->id);
		}

		getSubExpr(1)->process(compiler, scope);
	}

	COMPILER_PASS(BaseCompiler::DataInitialisation)
	{
		if (isFirstAssignment)
			initClassMembers(compiler, scope);
	}

	COMPILER_PASS(BaseCompiler::ResolvingSymbols)
	{
		switch (targetType)
		{
		case TargetType::Variable:
		{
			auto v = getTargetVariable();

			if (v->id.isConst() && !isFirstAssignment)
				throwError("Can't change constant variable");
		}
		case TargetType::Reference:
		{
			break;
		}
		case TargetType::ClassMember:
		{
			//...
			break;
		}
		case TargetType::Span:
		{
			// nothing to do...
			break;
		}
		}
	}


	COMPILER_PASS(BaseCompiler::TypeCheck)
	{
		auto targetIsSimd = SpanType::isSimdType(getSubExpr(1)->getComplexType());

		if (targetIsSimd)
		{
			type = Types::ID::Pointer;

			auto valueIsSimd = SpanType::isSimdType(getSubExpr(0)->getComplexType());

			if (!valueIsSimd)
				setTypeForChild(0, Types::ID::Float);

			
		}
		else
		{
			checkAndSetType(0, getSubExpr(1)->getType());
		}
		
	}

	COMPILER_PASS(BaseCompiler::CodeGeneration)
	{
		auto acg = CREATE_ASM_COMPILER(type);

		getSubExpr(0)->process(compiler, scope);
		getSubExpr(1)->process(compiler, scope);

		// jassertfalse;
		// here you should do something with the reference...

		auto value = getSubRegister(0);
		auto tReg = getSubRegister(1);

		if (targetType == TargetType::Reference && isFirstAssignment)
		{
			jassert(value->hasCustomMemoryLocation() || value->isMemoryLocation());



			tReg->setCustomMemoryLocation(value->getMemoryLocationForReference());
		}
		else
		{
			if (assignmentType == JitTokens::assign_)
			{
				if (tReg != value)
					acg.emitStore(tReg, value);
			}
			else
				acg.emitBinaryOp(assignmentType, tReg, value);

			if (targetType == TargetType::Variable)
			{
				if (auto wt = dynamic_cast<WrapType*>(getTargetVariable()->getComplexType().get()))
				{
					acg.emitWrap(wt, tReg, WrapType::OpType::Set);
				}
			}
		}
	}
}

void Operations::Assignment::initClassMembers(BaseCompiler* compiler, BaseScope* scope)
{
	if (getSubExpr(0)->isConstExpr() && scope->getScopeType() == BaseScope::Class)
	{
		auto target = getTargetVariable()->id;
		auto initValue = getSubExpr(0)->getConstExprValue();

		if (auto st = dynamic_cast<StructType*>(dynamic_cast<ClassScope*>(scope)->typePtr.get()))
		{
			auto ok = st->setDefaultValue(target.id, InitialiserList::makeSingleList(initValue));

			if (!ok)
				throwError("Can't initialise default value");
		}
		else
		{
			// this will initialise the class members to constant values...
			auto rd = scope->getRootClassScope()->rootData.get();

			auto ok = rd->initData(scope, target, InitialiserList::makeSingleList(initValue));

			if (!ok.wasOk())
				location.throwError(ok.getErrorMessage());
		}
	}
}

Operations::Assignment::Assignment(Location l, Expression::Ptr target, TokenType assignmentType_, Expression::Ptr expr, bool firstAssignment_) :
	Expression(l),
	assignmentType(assignmentType_),
	isFirstAssignment(firstAssignment_)
{
	if (auto v = dynamic_cast<VariableReference*>(target.get()))
	{
		targetType = v->id.isReference() ? TargetType::Reference : TargetType::Variable;
	}
	else if (dynamic_cast<DotOperator*>(target.get()))
		targetType = TargetType::ClassMember;
	else if (dynamic_cast<Subscript*>(target.get()))
		targetType = TargetType::Span;

	addStatement(expr);
	addStatement(target); // the target must be evaluated after the expression
}

snex::jit::Symbol Operations::BlockAccess::isWrappedBufferReference(Statement::Ptr expr, BaseScope* scope)
{
	if (auto var = dynamic_cast<Operations::VariableReference*>(expr.get()))
	{
		if (auto s = scope->getScopeForSymbol(var->id))
		{
			if (auto cs = dynamic_cast<ClassScope*>(s))
			{
				if (auto wrappedBuffer = dynamic_cast<WrappedBufferBase*>(cs->getSubFunctionClass(var->id)))
				{
					return var->id;
				}
			}
		}

	}

	return {};
}


void Operations::DotOperator::process(BaseCompiler* compiler, BaseScope* scope)
{
	Expression::process(compiler, scope);

	if (getDotChild()->isConstExpr())
	{
		replaceInParent(new Immediate(location, getDotChild()->getConstExprValue()));
		return;
	}
		
	COMPILER_PASS(BaseCompiler::TypeCheck)
	{
		if (auto fc = dynamic_cast<FunctionCall*>(getDotChild().get()))
		{
			jassertfalse;
			bool isPointer = getDotParent()->getType() == Types::ID::Pointer;

			if (!isPointer)
				throwError("Can't call non-object");
		}
	}

	COMPILER_PASS(BaseCompiler::CodeGeneration)
	{


		if (auto vp = dynamic_cast<VariableReference*>(getDotChild().get()))
		{
			reg = compiler->registerPool.getNextFreeRegister(scope, getType());
			reg->setReference(scope, vp->id);
			
			auto acg = CREATE_ASM_COMPILER(getType());
			acg.emitMemberAcess(reg, getSubRegister(0), getSubRegister(1));
		}
		else
		{
			jassertfalse;

			// just steal the register from the child...
			reg = getSubRegister(1);
			return;
		}
	}
}

}
}
