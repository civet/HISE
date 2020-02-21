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

class FunctionScope;

/** This class has a variable pool that will not exceed the lifetime of the compilation. */
class Operations
{
public:

	using FunctionCompiler = asmjit::X86Compiler;

	static FunctionCompiler& getFunctionCompiler(BaseCompiler* c);
	static BaseScope* findClassScope(BaseScope* scope);
	static BaseScope* findFunctionScope(BaseScope* scope);

	using RegPtr = AssemblyRegister::Ptr;

	static asmjit::Runtime* getRuntime(BaseCompiler* c);

	using Location = ParserHelpers::CodeLocation;
	using TokenType = ParserHelpers::TokenType;

	struct Statement : public ReferenceCountedObject
	{
		using Ptr = ReferenceCountedObjectPtr<Statement>;

		Statement(Location l);;

		void setParent(Statement* parent_)
		{
			parent = parent_;
		}

		template <class T> bool is() const
		{
			return dynamic_cast<const T*>(this) != nullptr;
		}

		virtual ~Statement() {};

		virtual Types::ID getType() const = 0;
		virtual void process(BaseCompiler* compiler, BaseScope* scope);

		virtual bool hasSideEffect() const { return false; }

		virtual size_t getRequiredByteSize(BaseCompiler* compiler, BaseScope* scope) const { return 0; }

		Ptr getChildStatement(int index) const
		{
			return childStatements[index];
		}

		int getNumChildStatements() const
		{
			return childStatements.size();
		}

		void swapSubExpressions(int first, int second)
		{
			childStatements.swap(first, second);
		}

		void flushChildStatements()
		{
			jassert(currentPass >= BaseCompiler::CodeGeneration);

			childStatements.clear();
		}

		void throwError(const juce::String& errorMessage);
		void logOptimisationMessage(const juce::String& m);
		void logWarning(const juce::String& m);
		void logMessage(BaseCompiler* compiler, BaseCompiler::MessageType type, const juce::String& message);

		Statement** begin() const { return childStatements.begin(); }

		Statement** end() const { return childStatements.end(); }

		Statement::Ptr getLastStatement() const
		{
			return childStatements.getLast();
		}

		void processAllChildren(BaseCompiler* compiler, BaseScope* scope)
		{
			for (auto s : *this)
				s->process(compiler, scope);
		}

		bool forEachRecursive(const std::function<bool(Ptr)>& f)
		{
			if (f(this))
				return true;

			for (auto c : *this)
			{
				if (c->forEachRecursive(f))
					return true;
			}

			return false;
		}

		virtual bool isConstExpr() const;

		Location location;

		BaseCompiler* currentCompiler = nullptr;
		BaseScope* currentScope = nullptr;
		BaseCompiler::Pass currentPass;

		WeakReference<Statement> parent;

		void addStatement(Statement* b, bool addFirst=false);

		Ptr replaceInParent(Ptr newExpression);
		Ptr replaceChildStatement(int index, Ptr newExpr);

	private:

		ReferenceCountedArray<Statement> childStatements;

		JUCE_DECLARE_WEAK_REFERENCEABLE(Statement);
		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Statement);
	};

	/** A high level node in the syntax tree that is used by the optimization passes
		to simplify the code.
	*/
	struct Expression : public Statement
	{
		using Ptr = ReferenceCountedObjectPtr<Expression>;

		Expression(Location loc) :
			Statement(loc)
		{};

		virtual ~Expression() {};

		Types::ID getType() const override;

		void attachAsmComment(const juce::String& message);

		void checkAndSetType(int offset = 0, Types::ID expectedType = Types::ID::Dynamic);

		/** Processes all sub expressions. Call this from your base class. */
		void process(BaseCompiler* compiler, BaseScope* scope) override;

		bool isAnonymousStatement() const;

		

		virtual VariableStorage getConstExprValue() const;

		virtual ComplexType::Ptr getComplexType() const
		{
			// you return a pointer, you return a complex type...
			jassert(getType() != Types::ID::Pointer);

			return nullptr;
		}

		bool hasSubExpr(int index) const;

		virtual VariableStorage getPointerValue() const;

		Ptr getSubExpr(int index) const;

		/** Returns a pointer to the register of this expression.

			This can be called after the RegisterAllocation pass
		*/
		RegPtr getSubRegister(int index) const;

		juce::String asmComment;

		RegPtr reg;

		void releaseRegister()
		{
			reg = nullptr;
		}

		void setTargetRegister(RegPtr targetToUse, bool clearRegister=true)
		{
			if (reg != nullptr)
				return;

			if (clearRegister)
			{
				jassert(targetToUse->canBeReused());

				targetToUse->clearForReuse();

				
			}

			reg = targetToUse;
		}


	protected:

		Types::ID type;

	private:

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Expression);
	};

	template <class T> static bool isStatementType(const Statement* t)
	{
		return dynamic_cast<const T*>(t) != nullptr;
	}

	template <class T> static T* findParentStatementOfType(Operations::Statement* e)
	{
		if (auto p = dynamic_cast<T*>(e))
			return p;

		if (e->parent != nullptr)
			return findParentStatementOfType<T>(e->parent.get());

		return nullptr;
	}

	template <class T> static const T* findParentStatementOfType(const Operations::Statement* e)
	{
		if (auto p = dynamic_cast<const T*>(e))
			return p;

		if (e->parent != nullptr)
			return findParentStatementOfType<T>(e->parent.get());

		return nullptr;
	}

	static bool isOpAssignment(Expression::Ptr p);


	struct Assignment;		struct Immediate;				struct Noop;
	struct FunctionCall;	struct ReturnStatement;			struct StatementBlock;
	struct Function;		struct BinaryOp;				struct VariableReference;
	struct TernaryOp;		struct LogicalNot;				struct Cast;
	struct Negation;		struct Compare;					struct UnaryOp;
	struct Increment;		struct BlockAccess;				struct BlockAssignment;
	struct Loop;		struct IfStatement;				struct SmoothedVariableDefinition;
	struct WrappedBlockDefinition;	struct ClassStatement;	struct ClassInstance;
	struct PointerReference;	struct DotOperator;			struct UsingStatement;


	struct SpanDefinition;	struct Subscript; struct SpanAssignment;

	struct WrapDefinition;


	struct InlinedExternalCall;

	struct ScopeStatementBase
	{
		virtual ~ScopeStatementBase() {};

		void addAlias(const Symbol& s)
		{
			aliases.add(s);
		}

		Types::ID getAliasNativeType(const Identifier& id) const
		{
			for (auto a : aliases)
			{
				if (a.id == id)
					return a.type;
			}

			return Types::ID::Void;
		}

		ComplexType::Ptr getAliasComplexType(const Identifier& id) const
		{
			for (auto a : aliases)
			{
				if (a.id == id)
					return a.typePtr;
			}

			return nullptr;
		}

		ScopeStatementBase* getScopedStatementForAlias(const Identifier& id)
		{
			for (auto a : aliases)
			{
				if (a.id == id)
					return this;
			}

			if (parentScopeStatement != nullptr)
				return parentScopeStatement->getScopedStatementForAlias(id);

			return nullptr;
		}

		void setParentScopeStatement(ScopeStatementBase* parent)
		{
			parentScopeStatement = parent;
		}

	private:

		WeakReference<ScopeStatementBase> parentScopeStatement;
		Array<Symbol> aliases;

		JUCE_DECLARE_WEAK_REFERENCEABLE(ScopeStatementBase);
	};

	/** Just a empty base class that checks whether the global variables will be loaded
		before the branch.
	*/
	struct ConditionalBranch
	{
		void allocateDirtyGlobalVariables(Statement::Ptr stament, BaseCompiler* c, BaseScope* s);

		

		virtual ~ConditionalBranch() {}
	};

	struct TypeDefinitionBase
	{
		virtual ~TypeDefinitionBase() {};

		virtual Identifier getInstanceId() const = 0;
		virtual ComplexType::Ptr getTypePtr() const = 0;
		virtual Types::ID getNativeType() const = 0;
	};

	struct BranchingStatement
	{
		virtual ~BranchingStatement() {};

		Statement::Ptr getCondition()
		{
			return asStatement()->getChildStatement(0);
		}

		Statement::Ptr getTrueBranch()
		{
			return asStatement()->getChildStatement(1);
		}

		Statement::Ptr getFalseBranch()
		{
			if (asStatement()->getNumChildStatements() > 2)
				return asStatement()->getChildStatement(2);

			return nullptr;
		}

		Statement* asStatement()
		{
			return dynamic_cast<Statement*>(this);
		}
	};

	static Expression* findAssignmentForVariable(Expression::Ptr variable, BaseScope* scope);
};

/** A syntax tree is a list of statements without a parent (so that the SyntaxTreeWalker will look deeper than that. 

It's used by either class definitions or function definition (so that each function has its own syntax tree).
*/
class SyntaxTree : public Operations::Statement,
				   public Operations::ScopeStatementBase
{
public:

	SyntaxTree(ParserHelpers::CodeLocation l);;
	Types::ID getType() const { return Types::ID::Void; }

	size_t getRequiredByteSize(BaseCompiler* compiler, BaseScope* scope) const
	{
		size_t s = 0;

		for (int i = 0; i < getNumChildStatements(); i++)
			s += getChildStatement(i)->getRequiredByteSize(compiler, scope);

		return s;
	}

	void process(BaseCompiler* compiler, BaseScope* scope) override
	{
		Statement::process(compiler, scope);

		for (int i = 0; i < getNumChildStatements(); i++)
			getChildStatement(i)->process(compiler, scope);
	}

	bool isFirstReference(Operations::Statement* v) const;

	void addAlias(const Identifier& id, const juce::String& typeString)
	{
		registeredAliases.add({ id, typeString });

	}

private:

	struct UsingAliases
	{
		Identifier id;
		juce::String aliasContent;
	};

	Array<UsingAliases> registeredAliases;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SyntaxTree);
};

}
}