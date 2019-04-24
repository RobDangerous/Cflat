
////////////////////////////////////////////////////////////////////////
//
//  Cflat v0.10
//  C++ compatible Scripting Language
//
//  Copyright (c) 2019 Arturo Cepeda P�rez
//
//  --------------------------------------------------------------------
//
//  This file is part of Cflat. Permission is hereby granted, free 
//  of charge, to any person obtaining a copy of this software and 
//  associated documentation files (the "Software"), to deal in the 
//  Software without restriction, including without limitation the 
//  rights to use, copy, modify, merge, publish, distribute, 
//  sublicense, and/or sell copies of the Software, and to permit 
//  persons to whom the Software is furnished to do so, subject to 
//  the following conditions:
//
//  The above copyright notice and this permission notice shall be 
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY 
//  KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
//  WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
//  PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS 
//  OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR 
//  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
//  OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////

#include "Cflat.h"

//
//  Global functions
//
namespace Cflat
{
   template<typename T>
   inline T min(T pA, T pB)
   {
      return pA < pB ? pA : pB;
   }
   template<typename T>
   inline T max(T pA, T pB)
   {
      return pA > pB ? pA : pB;
   }

   uint32_t hash(const char* pString)
   {
      const uint32_t kOffsetBasis = 2166136261u;
      const uint32_t kFNVPrime = 16777619u;

      uint32_t charIndex = 0u;
      uint32_t hash = kOffsetBasis;

      while(pString[charIndex] != '\0')
      {
         hash ^= pString[charIndex++];
         hash *= kFNVPrime;
      }

      return hash;
   }
}


//
//  AST Types
//
namespace Cflat
{
   enum class ExpressionType
   {
      Value,
      NullPointer,
      VariableAccess,
      MemberAccess,
      UnaryOperation,
      BinaryOperation,
      Parenthesized,
      AddressOf,
      Conditional,
      FunctionCall,
      MethodCall,
   };

   struct Expression
   {
   protected:
      ExpressionType mType;

      Expression()
      {
      }

   public:
      virtual ~Expression()
      {
      }

      ExpressionType getType() const
      {
         return mType;
      }
   };

   struct ExpressionNullPointer : Expression
   {
      ExpressionNullPointer()
      {
         mType = ExpressionType::NullPointer;
      }
   };

   struct ExpressionValue : Expression
   {
      Value mValue;

      ExpressionValue(const Value& pValue)
      {
         mType = ExpressionType::Value;

         mValue.initOnHeap(pValue.mTypeUsage);
         mValue.set(pValue.mValueBuffer);
      }
   };

   struct ExpressionVariableAccess : Expression
   {
      Identifier mVariableIdentifier;

      ExpressionVariableAccess(const Identifier& pVariableIdentifier)
         : mVariableIdentifier(pVariableIdentifier)
      {
         mType = ExpressionType::VariableAccess;
      }
   };

   struct ExpressionMemberAccess : Expression
   {
      CflatSTLVector<Identifier> mIdentifiers;

      ExpressionMemberAccess()
      {
         mType = ExpressionType::MemberAccess;
      }
   };

   struct ExpressionBinaryOperation : Expression
   {
      Expression* mLeft;
      Expression* mRight;
      char mOperator[4];

      ExpressionBinaryOperation(Expression* pLeft, Expression* pRight, const char* pOperator)
         : mLeft(pLeft)
         , mRight(pRight)
      {
         mType = ExpressionType::BinaryOperation;
         strcpy(mOperator, pOperator);
      }

      virtual ~ExpressionBinaryOperation()
      {
         if(mLeft)
         {
            CflatInvokeDtor(Expression, mLeft);
            CflatFree(mLeft);
         }

         if(mRight)
         {
            CflatInvokeDtor(Expression, mRight);
            CflatFree(mRight);
         }
      }
   };

   struct ExpressionParenthesized : Expression
   {
      Expression* mExpression;

      ExpressionParenthesized(Expression* pExpression)
         : mExpression(pExpression)
      {
         mType = ExpressionType::Parenthesized;
      }

      virtual ~ExpressionParenthesized()
      {
         if(mExpression)
         {
            CflatInvokeDtor(Expression, mExpression);
            CflatFree(mExpression);
         }
      }
   };

   struct ExpressionAddressOf : Expression
   {
      Expression* mExpression;

      ExpressionAddressOf(Expression* pExpression)
         : mExpression(pExpression)
      {
         mType = ExpressionType::AddressOf;
      }

      virtual ~ExpressionAddressOf()
      {
         if(mExpression)
         {
            CflatInvokeDtor(Expression, mExpression);
            CflatFree(mExpression);
         }
      }
   };

   struct ExpressionFunctionCall : Expression
   {
      Identifier mFunctionIdentifier;
      CflatSTLVector<Expression*> mArguments;

      ExpressionFunctionCall(const Identifier& pFunctionIdentifier)
         : mFunctionIdentifier(pFunctionIdentifier)
      {
         mType = ExpressionType::FunctionCall;
      }

      virtual ~ExpressionFunctionCall()
      {
         for(size_t i = 0u; i < mArguments.size(); i++)
         {
            CflatInvokeDtor(Expression, mArguments[i]);
            CflatFree(mArguments[i]);
         }
      }
   };

   struct ExpressionMethodCall : Expression
   {
      Expression* mMemberAccess;
      CflatSTLVector<Expression*> mArguments;

      ExpressionMethodCall(Expression* pMemberAccess)
         : mMemberAccess(pMemberAccess)
      {
         mType = ExpressionType::MethodCall;
      }

      virtual ~ExpressionMethodCall()
      {
         if(mMemberAccess)
         {
            CflatInvokeDtor(Expression, mMemberAccess);
            CflatFree(mMemberAccess);
         }

         for(size_t i = 0u; i < mArguments.size(); i++)
         {
            CflatInvokeDtor(Expression, mArguments[i]);
            CflatFree(mArguments[i]);
         }
      }
   };


   enum class StatementType
   {
      Expression,
      Block,
      UsingDirective,
      NamespaceDeclaration,
      VariableDeclaration,
      FunctionDeclaration,
      Assignment,
      Increment,
      Decrement,
      If,
      While,
      For,
      Break,
      Continue,
      Return
   };

   struct Statement
   {
   protected:
      StatementType mType;

      Statement()
         : mLine(0u)
      {
      }

   public:
      uint16_t mLine;

      virtual ~Statement()
      {
      }

      StatementType getType() const
      {
         return mType;
      }
   };

   struct StatementExpression : Statement
   {
      Expression* mExpression;

      StatementExpression(Expression* pExpression)
         : mExpression(pExpression)
      {
         mType = StatementType::Expression;
      }

      virtual ~StatementExpression()
      {
         if(mExpression)
         {
            CflatInvokeDtor(Expression, mExpression);
            CflatFree(mExpression);
         }
      }
   };

   struct StatementBlock : Statement
   {
      CflatSTLVector<Statement*> mStatements;

      StatementBlock()
      {
         mType = StatementType::Block;
      }

      virtual ~StatementBlock()
      {
         for(size_t i = 0u; i < mStatements.size(); i++)
         {
            CflatInvokeDtor(Statement, mStatements[i]);
            CflatFree(mStatements[i]);
         }
      }
   };

   struct StatementVariableDeclaration : Statement
   {
      TypeUsage mTypeUsage;
      Identifier mVariableIdentifier;
      Expression* mInitialValue;

      StatementVariableDeclaration(const TypeUsage& pTypeUsage, const Identifier& pVariableIdentifier,
         Expression* pInitialValue)
         : mTypeUsage(pTypeUsage)
         , mVariableIdentifier(pVariableIdentifier)
         , mInitialValue(pInitialValue)
      {
         mType = StatementType::VariableDeclaration;
      }

      virtual ~StatementVariableDeclaration()
      {
         if(mInitialValue)
         {
            CflatInvokeDtor(Expression, mInitialValue);
            CflatFree(mInitialValue);
         }
      }
   };

   struct StatementFunctionDeclaration : Statement
   {
      TypeUsage mReturnType;
      Identifier mFunctionIdentifier;
      CflatSTLVector<Identifier> mParameterIdentifiers;
      CflatSTLVector<TypeUsage> mParameterTypes;
      StatementBlock* mBody;

      StatementFunctionDeclaration(const TypeUsage& pReturnType, const Identifier& pFunctionIdentifier)
         : mReturnType(pReturnType)
         , mFunctionIdentifier(pFunctionIdentifier)
         , mBody(nullptr)
      {
         mType = StatementType::FunctionDeclaration;
      }

      virtual ~StatementFunctionDeclaration()
      {
         if(mBody)
         {
            CflatInvokeDtor(StatementBlock, mBody);
            CflatFree(mBody);
         }
      }
   };

   struct StatementAssignment : Statement
   {
      Expression* mLeftValue;
      Expression* mRightValue;
      char mOperator[4];

      StatementAssignment(Expression* pLeftValue, Expression* pRightValue, const char* pOperator)
         : mLeftValue(pLeftValue)
         , mRightValue(pRightValue)
      {
         mType = StatementType::Assignment;
         strcpy(mOperator, pOperator);
      }

      virtual ~StatementAssignment()
      {
         if(mLeftValue)
         {
            CflatInvokeDtor(Expression, mLeftValue);
            CflatFree(mLeftValue);
         }

         if(mRightValue)
         {
            CflatInvokeDtor(Expression, mRightValue);
            CflatFree(mRightValue);
         }
      }
   };

   struct StatementIncrement : Statement
   {
      Identifier mVariableIdentifier;

      StatementIncrement(const Identifier& pVariableIdentifier)
         : mVariableIdentifier(pVariableIdentifier)
      {
         mType = StatementType::Increment;
      }
   };

   struct StatementDecrement : Statement
   {
      Identifier mVariableIdentifier;

      StatementDecrement(const Identifier& pVariableIdentifier)
         : mVariableIdentifier(pVariableIdentifier)
      {
         mType = StatementType::Decrement;
      }
   };

   struct StatementIf : Statement
   {
      Expression* mCondition;
      Statement* mIfStatement;
      Statement* mElseStatement;

      StatementIf(Expression* pCondition, Statement* pIfStatement, Statement* pElseStatement)
         : mCondition(pCondition)
         , mIfStatement(pIfStatement)
         , mElseStatement(pElseStatement)
      {
         mType = StatementType::If;
      }

      virtual ~StatementIf()
      {
         if(mCondition)
         {
            CflatInvokeDtor(Expression, mCondition);
            CflatFree(mCondition);
         }

         if(mIfStatement)
         {
            CflatInvokeDtor(Statement, mIfStatement);
            CflatFree(mIfStatement);
         }

         if(mElseStatement)
         {
            CflatInvokeDtor(Statement, mElseStatement);
            CflatFree(mElseStatement);
         }
      }
   };

   struct StatementWhile : Statement
   {
      Expression* mCondition;
      Statement* mLoopStatement;

      StatementWhile(Expression* pCondition, Statement* pLoopStatement)
         : mCondition(pCondition)
         , mLoopStatement(pLoopStatement)
      {
         mType = StatementType::While;
      }

      virtual ~StatementWhile()
      {
         if(mCondition)
         {
            CflatInvokeDtor(Expression, mCondition);
            CflatFree(mCondition);
         }

         if(mLoopStatement)
         {
            CflatInvokeDtor(Statement, mLoopStatement);
            CflatFree(mLoopStatement);
         }
      }
   };

   struct StatementFor : Statement
   {
      Statement* mInitialization;
      Expression* mCondition;
      Statement* mIncrement;
      Statement* mLoopStatement;

      StatementFor(Statement* pInitialization, Expression* pCondition, Statement* pIncrement,
         Statement* pLoopStatement)
         : mInitialization(pInitialization)
         , mCondition(pCondition)
         , mIncrement(pIncrement)
         , mLoopStatement(pLoopStatement)
      {
         mType = StatementType::For;
      }

      virtual ~StatementFor()
      {
         if(mInitialization)
         {
            CflatInvokeDtor(Statement, mInitialization);
            CflatFree(mInitialization);
         }

         if(mCondition)
         {
            CflatInvokeDtor(Expression, mCondition);
            CflatFree(mCondition);
         }

         if(mIncrement)
         {
            CflatInvokeDtor(Statement, mIncrement);
            CflatFree(mIncrement);
         }

         if(mLoopStatement)
         {
            CflatInvokeDtor(Statement, mLoopStatement);
            CflatFree(mLoopStatement);
         }
      }
   };

   struct StatementBreak : Statement
   {
      StatementBreak()
      {
         mType = StatementType::Break;
      }
   };

   struct StatementContinue : Statement
   {
      StatementContinue()
      {
         mType = StatementType::Continue;
      }
   };

   struct StatementReturn : Statement
   {
      Expression* mExpression;

      StatementReturn(Expression* pExpression)
         : mExpression(pExpression)
      {
         mType = StatementType::Return;
      }

      virtual ~StatementReturn()
      {
         if(mExpression)
         {
            CflatInvokeDtor(Expression, mExpression);
            CflatFree(mExpression);
         }
      }
   };


   const char* kCompileErrorStrings[] = 
   {
      "unexpected symbol after '%s'",
      "undefined variable ('%s')",
      "variable redefinition ('%s')",
      "no default constructor defined for the '%s' type",
      "invalid member access operator ('%s' is a pointer)",
      "invalid member access operator ('%s' is not a pointer)",
      "invalid operator for the '%s' type",
      "no member named '%s'",
      "'%s' must be an integer value"
   };
   const size_t kCompileErrorStringsCount = sizeof(kCompileErrorStrings) / sizeof(const char*);

   const char* kRuntimeErrorStrings[] = 
   {
      "null pointer access ('%s')",
      "invalid array index ('%s')",
      "division by zero"
   };
   const size_t kRuntimeErrorStringsCount = sizeof(kRuntimeErrorStrings) / sizeof(const char*);
}


//
//  Program
//
using namespace Cflat;

Program::~Program()
{
   for(size_t i = 0u; i < mStatements.size(); i++)
   {
      CflatInvokeDtor(Statement, mStatements[i]);
      CflatFree(mStatements[i]);
   }
}


//
//  Namespace
//
Namespace::Namespace(const Identifier& pIdentifier)
   : mName(pIdentifier)
{
}

Namespace::~Namespace()
{
   for(NamespacesRegistry::iterator it = mNamespaces.begin(); it != mNamespaces.end(); it++)
   {
      Namespace* ns = it->second;
      CflatInvokeDtor(Namespace, ns);
      CflatFree(ns);
   }

   for(FunctionsRegistry::iterator it = mFunctions.begin(); it != mFunctions.end(); it++)
   {
      CflatSTLVector<Function*>& functions = it->second;

      for(size_t i = 0u; i < functions.size(); i++)
      {
         Function* function = functions[i];
         CflatInvokeDtor(Function, function);
         CflatFree(function);
      }
   }

   for(TypesRegistry::iterator it = mTypes.begin(); it != mTypes.end(); it++)
   {
      Type* type = it->second;
      CflatInvokeDtor(Type, type);
      CflatFree(type);
   }
}

Type* Namespace::getType(const Identifier& pIdentifier)
{
   TypesRegistry::const_iterator it = mTypes.find(pIdentifier.mHash);
   return it != mTypes.end() ? it->second : nullptr;
}

Function* Namespace::getFunction(const Identifier& pIdentifier)
{
   FunctionsRegistry::iterator it = mFunctions.find(pIdentifier.mHash);
   return it != mFunctions.end() ? it->second.at(0) : nullptr;
}

CflatSTLVector<Function*>* Namespace::getFunctions(const Identifier& pIdentifier)
{
   FunctionsRegistry::iterator it = mFunctions.find(pIdentifier.mHash);
   return it != mFunctions.end() ? &it->second : nullptr;
}

Function* Namespace::registerFunction(const Identifier& pIdentifier)
{
   Function* function = (Function*)CflatMalloc(sizeof(Function));
   CflatInvokeCtor(Function, function)(pIdentifier);
   FunctionsRegistry::iterator it = mFunctions.find(pIdentifier.mHash);

   if(it == mFunctions.end())
   {
      CflatSTLVector<Function*> functions;
      functions.push_back(function);
      mFunctions[pIdentifier.mHash] = functions;
   }
   else
   {
      it->second.push_back(function);
   }

   return function;
}

void Namespace::setVariable(const TypeUsage& pTypeUsage, const Identifier& pIdentifier, const Value& pValue)
{
   Instance* instance = retrieveInstance(pIdentifier);

   if(!instance)
   {
      instance = registerInstance(pTypeUsage, pIdentifier);
   }

   instance->mValue.initOnHeap(pTypeUsage);
   instance->mValue.set(pValue.mValueBuffer);
}

Value* Namespace::getVariable(const Identifier& pIdentifier)
{
   Instance* instance = retrieveInstance(pIdentifier);
   return instance ? &instance->mValue : nullptr;
}

Instance* Namespace::registerInstance(const TypeUsage& pTypeUsage, const Identifier& pIdentifier)
{
   Instance instance(pTypeUsage, pIdentifier);
   mInstances.push_back(instance);
   return &mInstances.back();
}

Instance* Namespace::retrieveInstance(const Identifier& pIdentifier)
{
   Instance* instance = nullptr;

   for(int i = (int)mInstances.size() - 1; i >= 0; i--)
   {
      if(mInstances[i].mIdentifier == pIdentifier)
      {
         instance = &mInstances[i];
         break;
      }
   }

   return instance;
}

void Namespace::releaseInstances(uint32_t pScopeLevel)
{
   while(!mInstances.empty() && mInstances.back().mScopeLevel >= pScopeLevel)
   {
      mInstances.pop_back();
   }

   for(NamespacesRegistry::iterator it = mNamespaces.begin(); it != mNamespaces.end(); it++)
   {
      Namespace* ns = it->second;
      ns->releaseInstances(pScopeLevel);
   }
}



//
//  Environment
//
const char* kCflatPunctuation[] = 
{
   ".", ",", ":", ";", "->", "(", ")", "{", "}", "[", "]", "::"
};
const size_t kCflatPunctuationCount = sizeof(kCflatPunctuation) / sizeof(const char*);

const char* kCflatOperators[] = 
{
   "+", "-", "*", "/",
   "++", "--", "!",
   "=", "+=", "-=", "*=", "/=",
   "==", "!=", ">", "<", ">=", "<=",
   "&&", "||", "&", "|", "~", "^"
};
const size_t kCflatOperatorsCount = sizeof(kCflatOperators) / sizeof(const char*);

const char* kCflatAssignmentOperators[] = 
{
   "=", "+=", "-=", "*=", "/="
};
const size_t kCflatAssignmentOperatorsCount = sizeof(kCflatAssignmentOperators) / sizeof(const char*);

const char* kCflatKeywords[] =
{
   "break", "case", "class", "const", "const_cast", "continue", "default",
   "delete", "do", "dynamic_cast", "else", "enum", "false", "for", "if",
   "namespace", "new", "nullptr", "operator", "private", "protected", "public",
   "reinterpret_cast", "return", "sizeof", "static", "static_cast",
   "struct", "switch", "this", "true", "typedef", "union", "unsigned",
   "using", "virtual", "void", "while"
};
const size_t kCflatKeywordsCount = sizeof(kCflatKeywords) / sizeof(const char*);

Environment::Environment()
   : mGlobalNamespace("")
{
   static_assert(kCompileErrorStringsCount == (size_t)Environment::CompileError::Count,
      "Missing compile error strings");
   static_assert(kRuntimeErrorStringsCount == (size_t)Environment::RuntimeError::Count,
      "Missing runtime error strings");

   registerBuiltInTypes();
}

Environment::~Environment()
{
}

void Environment::registerBuiltInTypes()
{
   CflatRegisterBuiltInType(this, int);
   CflatRegisterBuiltInType(this, uint32_t);
   CflatRegisterBuiltInType(this, size_t);
   CflatRegisterBuiltInType(this, char);
   CflatRegisterBuiltInType(this, bool);
   CflatRegisterBuiltInType(this, uint8_t);
   CflatRegisterBuiltInType(this, short);
   CflatRegisterBuiltInType(this, uint16_t);
   CflatRegisterBuiltInType(this, float);
   CflatRegisterBuiltInType(this, double);
}

TypeUsage Environment::parseTypeUsage(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;
   size_t cachedTokenIndex = tokenIndex;

   TypeUsage typeUsage;
   char baseTypeName[128];

   pContext.mStringBuffer.assign(tokens[tokenIndex].mStart, tokens[tokenIndex].mLength);

   while(tokens[tokenIndex + 1].mLength == 2u && strncmp(tokens[tokenIndex + 1].mStart, "::", 2u) == 0)
   {
      tokenIndex += 2u;
      pContext.mStringBuffer.append("::");
      pContext.mStringBuffer.append(tokens[tokenIndex].mStart, tokens[tokenIndex].mLength);
   }

   strcpy(baseTypeName, pContext.mStringBuffer.c_str());
   Type* type = getType(baseTypeName);

   if(!type)
   {
      for(size_t i = 0u; i < pContext.mUsingNamespaces.size(); i++)
      {
         pContext.mStringBuffer.assign(pContext.mUsingNamespaces[i]);
         pContext.mStringBuffer.append("::");
         pContext.mStringBuffer.append(baseTypeName);
         type = getType(pContext.mStringBuffer.c_str());

         if(type)
            break;
      }
   }

   if(type)
   {
      pContext.mStringBuffer.clear();

      const bool isConst = tokenIndex > 0u && strncmp(tokens[tokenIndex - 1u].mStart, "const", 5u) == 0;
      const bool isPointer = *tokens[tokenIndex + 1u].mStart == '*';
      const bool isReference = *tokens[tokenIndex + 1u].mStart == '&';

      if(isConst)
      {
         pContext.mStringBuffer.append("const ");
      }

      pContext.mStringBuffer.append(type->mIdentifier.mName);

      if(isPointer)
      {
         pContext.mStringBuffer.append("*");
         tokenIndex++;
      }
      else if(isReference)
      {
         pContext.mStringBuffer.append("&");
         tokenIndex++;
      }

      typeUsage = getTypeUsage(pContext.mStringBuffer.c_str());
   }
   else
   {
      tokenIndex = cachedTokenIndex;
   }

   return typeUsage;
}

void Environment::throwCompileError(ParsingContext& pContext, CompileError pError,
   const char* pArg1, const char* pArg2)
{
   const Token& token = pContext.mTokens[pContext.mTokenIndex];

   char errorMsg[256];
   sprintf(errorMsg, kCompileErrorStrings[(int)pError], pArg1, pArg2);

   pContext.mErrorMessage.assign("[Compile Error] Line ");
   pContext.mErrorMessage.append(std::to_string(token.mLine));
   pContext.mErrorMessage.append(": ");
   pContext.mErrorMessage.append(errorMsg);
}

void Environment::preprocess(ParsingContext& pContext, const char* pCode)
{
   CflatSTLString& preprocessedCode = pContext.mPreprocessedCode;

   const size_t codeLength = strlen(pCode);
   preprocessedCode.clear();

   size_t cursor = 0u;

   while(cursor < codeLength)
   {
      // line comment
      if(strncmp(pCode + cursor, "//", 2u) == 0)
      {
         while(pCode[cursor] != '\n' && pCode[cursor] != '\0')
         {
            cursor++;
         }
      }
      // block comment
      else if(strncmp(pCode + cursor, "/*", 2u) == 0)
      {
         while(strncmp(pCode + cursor, "*/", 2u) != 0)
         {
            cursor++;

            if(pCode[cursor] == '\n')
            {
               preprocessedCode.push_back('\n');
            }
         }

         continue;
      }
      // preprocessor directive
      else if(pCode[cursor] == '#')
      {
         //TODO
         while(pCode[cursor] != '\n' && pCode[cursor] != '\0')
         {
            cursor++;
         }
      }

      preprocessedCode.push_back(pCode[cursor++]);
   }

   if(preprocessedCode.back() != '\n')
   {
      preprocessedCode.push_back('\n');
   }

   preprocessedCode.shrink_to_fit();
}

void Environment::tokenize(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;

   char* cursor = const_cast<char*>(pContext.mPreprocessedCode.c_str());
   uint16_t currentLine = 1u;

   tokens.clear();

   while(*cursor != '\0')
   {
      while(*cursor == ' ' || *cursor == '\n')
      {
         if(*cursor == '\n')
            currentLine++;

         cursor++;
      }

      if(*cursor == '\0')
         return;

      Token token;
      token.mStart = cursor;
      token.mLength = 1u;
      token.mLine = currentLine;

      // string
      if(*cursor == '"')
      {
         do
         {
            cursor++;
         }
         while(!(*cursor == '"' && *(cursor - 1) != '\\'));

         cursor++;
         token.mLength = cursor - token.mStart;
         token.mType = TokenType::String;
         tokens.push_back(token);
         continue;
      }

      // numeric value
      if(isdigit(*cursor))
      {
         do
         {
            cursor++;
         }
         while(isdigit(*cursor) || *cursor == '.' || *cursor == 'f' || *cursor == 'x' || *cursor == 'u');

         token.mLength = cursor - token.mStart;
         token.mType = TokenType::Number;
         tokens.push_back(token);
         continue;
      }

      // punctuation (2 characters)
      const size_t tokensCount = tokens.size();

      for(size_t i = 0u; i < kCflatPunctuationCount; i++)
      {
         if(strncmp(token.mStart, kCflatPunctuation[i], 2u) == 0)
         {
            cursor += 2;
            token.mLength = cursor - token.mStart;
            token.mType = TokenType::Punctuation;
            tokens.push_back(token);
            break;
         }
      }

      if(tokens.size() > tokensCount)
         continue;

      // operator (2 characters)
      for(size_t i = 0u; i < kCflatOperatorsCount; i++)
      {
         if(strncmp(token.mStart, kCflatOperators[i], 2u) == 0)
         {
            cursor += 2;
            token.mLength = cursor - token.mStart;
            token.mType = TokenType::Operator;
            tokens.push_back(token);
            break;
         }
      }

      if(tokens.size() > tokensCount)
         continue;

      // punctuation (1 character)
      for(size_t i = 0u; i < kCflatPunctuationCount; i++)
      {
         if(token.mStart[0] == kCflatPunctuation[i][0] && kCflatPunctuation[i][1] == '\0')
         {
            cursor++;
            token.mType = TokenType::Punctuation;
            tokens.push_back(token);
            break;
         }
      }

      if(tokens.size() > tokensCount)
         continue;

      // operator (1 character)
      for(size_t i = 0u; i < kCflatOperatorsCount; i++)
      {
         if(token.mStart[0] == kCflatOperators[i][0])
         {
            cursor++;
            token.mType = TokenType::Operator;
            tokens.push_back(token);
            break;
         }
      }

      if(tokens.size() > tokensCount)
         continue;

      // keywords
      for(size_t i = 0u; i < kCflatKeywordsCount; i++)
      {
         const size_t keywordLength = strlen(kCflatKeywords[i]);

         if(strncmp(token.mStart, kCflatKeywords[i], keywordLength) == 0)
         {
            cursor += keywordLength;
            token.mLength = cursor - token.mStart;
            token.mType = TokenType::Keyword;
            tokens.push_back(token);
            break;
         }
      }

      if(tokens.size() > tokensCount)
         continue;

      // identifier
      do
      {
         cursor++;
      }
      while(isalnum(*cursor) || *cursor == '_');

      token.mLength = cursor - token.mStart;
      token.mType = TokenType::Identifier;
      tokens.push_back(token);
   }
}

void Environment::parse(ParsingContext& pContext, Program& pProgram)
{
   size_t& tokenIndex = pContext.mTokenIndex;

   for(tokenIndex = 0u; tokenIndex < pContext.mTokens.size(); tokenIndex++)
   {
      Statement* statement = parseStatement(pContext);

      if(statement)
      {
         pProgram.mStatements.push_back(statement);
      }

      if(!pContext.mErrorMessage.empty())
         break;
   }
}

Expression* Environment::parseExpression(ParsingContext& pContext, size_t pTokenLastIndex)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;
   const Token& token = tokens[tokenIndex];
   Expression* expression = nullptr;

   const size_t tokensCount = pTokenLastIndex - pContext.mTokenIndex + 1u;

   if(tokensCount == 1u)
   {
      if(token.mType == TokenType::Number)
      {
         TypeUsage typeUsage;
         Value value;

         pContext.mStringBuffer.assign(token.mStart, token.mLength);
         const char* numberStr = pContext.mStringBuffer.c_str();
         const size_t numberStrLength = strlen(numberStr);

         // decimal value
         if(strchr(numberStr, '.'))
         {
            // float
            if(numberStr[numberStrLength - 1u] == 'f')
            {
               typeUsage.mType = getType("float");
               const float number = (float)strtod(numberStr, nullptr);
               value.initOnStack(typeUsage, &pContext.mStack);
               value.set(&number);
            }
            // double
            else
            {
               typeUsage.mType = getType("double");
               const double number = strtod(numberStr, nullptr);
               value.initOnStack(typeUsage, &pContext.mStack);
               value.set(&number);
            }
         }
         // integer value
         else
         {
            // unsigned
            if(numberStr[numberStrLength - 1u] == 'u')
            {
               typeUsage.mType = getType("uint32_t");
               const uint32_t number = (uint32_t)atoi(numberStr);
               value.initOnStack(typeUsage, &pContext.mStack);
               value.set(&number);
            }
            // signed
            else
            {
               typeUsage.mType = getType("int");
               const int number = atoi(numberStr);
               value.initOnStack(typeUsage, &pContext.mStack);
               value.set(&number);
            }
         }

         expression = (ExpressionValue*)CflatMalloc(sizeof(ExpressionValue));
         CflatInvokeCtor(ExpressionValue, expression)(value);
      }
      else if(token.mType == TokenType::String)
      {
         pContext.mStringBuffer.assign(token.mStart + 1, token.mLength - 1u);
         pContext.mStringBuffer[token.mLength - 2u] = '\0';

         const char* string =
            mLiteralStringsPool.push(pContext.mStringBuffer.c_str(), token.mLength - 1u);

         TypeUsage typeUsage = getTypeUsage("const char*");
         Value value;
         value.initOnStack(typeUsage, &pContext.mStack);
         value.set(&string);

         expression = (ExpressionValue*)CflatMalloc(sizeof(ExpressionValue));
         CflatInvokeCtor(ExpressionValue, expression)(value);
      }
      else if(token.mType == TokenType::Identifier)
      {
         // variable access
         pContext.mStringBuffer.assign(token.mStart, token.mLength);
         const Identifier identifier(pContext.mStringBuffer.c_str());

         Instance* instance = retrieveInstance(pContext, identifier);

         if(instance)
         {
            expression = (ExpressionVariableAccess*)CflatMalloc(sizeof(ExpressionVariableAccess));
            CflatInvokeCtor(ExpressionVariableAccess, expression)(identifier);  
         }
         else
         {
            throwCompileError(pContext, CompileError::UndefinedVariable, identifier.mName);
         }
      }
      else if(token.mType == TokenType::Keyword)
      {
         if(strncmp(token.mStart, "nullptr", 7u) == 0)
         {
            expression = (ExpressionNullPointer*)CflatMalloc(sizeof(ExpressionNullPointer));
            CflatInvokeCtor(ExpressionNullPointer, expression)();
         }
      }
   }
   else
   {
      size_t operatorTokenIndex = 0u;
      uint32_t parenthesisLevel = tokens[tokenIndex].mStart[0] == '(' ? 1u : 0u;

      for(size_t i = tokenIndex + 1u; i < pTokenLastIndex; i++)
      {
         if(tokens[i].mType == TokenType::Operator && parenthesisLevel == 0u)
         {
            operatorTokenIndex = i;
            break;
         }

         if(tokens[i].mStart[0] == '(')
         {
            parenthesisLevel++;
         }
         else if(tokens[i].mStart[0] == ')')
         {
            parenthesisLevel--;
         }
      }

      // binary operator
      if(operatorTokenIndex > 0u)
      {
         Expression* left = parseExpression(pContext, operatorTokenIndex - 1u);
         TypeUsage typeUsage = getTypeUsage(pContext, left);

         const Token& operatorToken = pContext.mTokens[operatorTokenIndex];
         CflatSTLString operatorStr(operatorToken.mStart, operatorToken.mLength);

         bool operatorIsValid = true;

         if(typeUsage.mType->mCategory != TypeCategory::BuiltIn)
         {
            pContext.mStringBuffer.assign("operator");
            pContext.mStringBuffer.append(operatorStr);
            Method* operatorMethod = findMethod(typeUsage.mType, pContext.mStringBuffer.c_str());

            if(!operatorMethod)
            {
               const char* typeName = typeUsage.mType->mIdentifier.mName;
               throwCompileError(pContext, CompileError::InvalidOperator, typeName, operatorStr.c_str());
               operatorIsValid = false;
            }
         }

         if(operatorIsValid)
         {
            tokenIndex = operatorTokenIndex + 1u;
            Expression* right = parseExpression(pContext, pTokenLastIndex);

            expression = (ExpressionBinaryOperation*)CflatMalloc(sizeof(ExpressionBinaryOperation));
            CflatInvokeCtor(ExpressionBinaryOperation, expression)(left, right, operatorStr.c_str());
         }
      }
      // parenthesized expression
      else if(tokens[tokenIndex].mStart[0] == '(')
      {
         const size_t closureTokenIndex = findClosureTokenIndex(pContext, '(', ')');
         tokenIndex++;

         expression = (ExpressionParenthesized*)CflatMalloc(sizeof(ExpressionParenthesized));
         CflatInvokeCtor(ExpressionParenthesized, expression)(parseExpression(pContext, closureTokenIndex - 1u));
         tokenIndex = closureTokenIndex + 1u;
      }
      else if(token.mType == TokenType::Identifier)
      {
         const Token& nextToken = tokens[tokenIndex + 1u];

         // function call
         if(nextToken.mStart[0] == '(')
         {
            pContext.mStringBuffer.assign(token.mStart, token.mLength);
            Identifier identifier(pContext.mStringBuffer.c_str());

            ExpressionFunctionCall* castedExpression = 
               (ExpressionFunctionCall*)CflatMalloc(sizeof(ExpressionFunctionCall));
            CflatInvokeCtor(ExpressionFunctionCall, castedExpression)(identifier);
            expression = castedExpression;

            tokenIndex++;
            parseFunctionCallArguments(pContext, castedExpression->mArguments);
         }
         // member access
         else if(nextToken.mStart[0] == '.' || strncmp(nextToken.mStart, "->", 2u) == 0)
         {
            ExpressionMemberAccess* castedExpression =
               (ExpressionMemberAccess*)CflatMalloc(sizeof(ExpressionMemberAccess));
            CflatInvokeCtor(ExpressionMemberAccess, castedExpression)();
            expression = castedExpression;

            parseMemberAccessIdentifiers(pContext, castedExpression->mIdentifiers);
         }
         // static member access
         else if(strncmp(nextToken.mStart, "::", 2u) == 0)
         {
            pContext.mStringBuffer.assign(token.mStart, token.mLength);

            while(strncmp(tokens[++tokenIndex].mStart, "::", 2u) == 0)
            {
               tokenIndex++;
               pContext.mStringBuffer.append("::");
               pContext.mStringBuffer.append(tokens[tokenIndex].mStart, tokens[tokenIndex].mLength);
            }

            const Identifier staticMemberIdentifier(pContext.mStringBuffer.c_str());

            // static method call
            if(tokens[tokenIndex].mStart[0] == '(')
            {
               Identifier identifier(pContext.mStringBuffer.c_str());

               ExpressionFunctionCall* castedExpression = 
                  (ExpressionFunctionCall*)CflatMalloc(sizeof(ExpressionFunctionCall));
               CflatInvokeCtor(ExpressionFunctionCall, castedExpression)(identifier);
               expression = castedExpression;

               parseFunctionCallArguments(pContext, castedExpression->mArguments);
            }
            // static member access
            else
            {
               expression = (ExpressionVariableAccess*)CflatMalloc(sizeof(ExpressionVariableAccess));
               CflatInvokeCtor(ExpressionVariableAccess, expression)(staticMemberIdentifier);
            }
         }
      }
      else if(token.mType == TokenType::Operator)
      {
         // address of
         if(token.mStart[0] == '&')
         {
            tokenIndex++;

            const size_t lastTokenIndex =
               Cflat::min(pTokenLastIndex, findClosureTokenIndex(pContext, ' ', ';') - 1u);

            expression = (ExpressionAddressOf*)CflatMalloc(sizeof(ExpressionAddressOf));
            CflatInvokeCtor(ExpressionAddressOf, expression)(parseExpression(pContext, lastTokenIndex));
         }
      }
   }

   return expression;
}

size_t Environment::findClosureTokenIndex(ParsingContext& pContext, char pOpeningChar, char pClosureChar)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t closureTokenIndex = 0u;

   if(tokens[pContext.mTokenIndex].mStart[0] == pClosureChar)
   {
      closureTokenIndex = pContext.mTokenIndex;
   }
   else
   {
      uint32_t scopeLevel = 0u;

      for(size_t i = (pContext.mTokenIndex + 1u); i < pContext.mTokens.size(); i++)
      {
         if(tokens[i].mStart[0] == pClosureChar)
         {
            if(scopeLevel == 0u)
            {
               closureTokenIndex = i;
               break;
            }

            scopeLevel--;
         }
         else if(tokens[i].mStart[0] == pOpeningChar)
         {
            scopeLevel++;
         }
      }
   }

   return closureTokenIndex;
}

TypeUsage Environment::getTypeUsage(ParsingContext& pContext, Expression* pExpression)
{
   TypeUsage typeUsage;

   switch(pExpression->getType())
   {
   case ExpressionType::Value:
      {
         ExpressionValue* expression = static_cast<ExpressionValue*>(pExpression);
         typeUsage = expression->mValue.mTypeUsage;
      }
      break;
   case ExpressionType::VariableAccess:
      {
         ExpressionVariableAccess* expression = static_cast<ExpressionVariableAccess*>(pExpression);
         Instance* instance = retrieveInstance(pContext, expression->mVariableIdentifier);
         typeUsage = instance->mTypeUsage;
      }
      break;
   case ExpressionType::BinaryOperation:
      {
         ExpressionBinaryOperation* expression = static_cast<ExpressionBinaryOperation*>(pExpression);
         typeUsage = getTypeUsage(pContext, expression->mLeft);
      }
      break;
   case ExpressionType::Parenthesized:
      {
         ExpressionParenthesized* expression = static_cast<ExpressionParenthesized*>(pExpression);
         typeUsage = getTypeUsage(pContext, expression->mExpression);
      }
      break;
   case ExpressionType::AddressOf:
      {
         ExpressionAddressOf* expression = static_cast<ExpressionAddressOf*>(pExpression);
         typeUsage = getTypeUsage(pContext, expression->mExpression);
         typeUsage.mPointerLevel++;
      }
      break;
   case ExpressionType::FunctionCall:
      {
         ExpressionFunctionCall* expression = static_cast<ExpressionFunctionCall*>(pExpression);
         Function* function = getFunction(expression->mFunctionIdentifier.mName);
         typeUsage = function->mReturnTypeUsage;
      }
      break;
   default:
      break;
   }

   return typeUsage;
}

Statement* Environment::parseStatement(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;
   const Token& token = tokens[tokenIndex];

   Statement* statement = nullptr;
   const uint16_t statementLine = token.mLine;

   switch(token.mType)
   {
      case TokenType::Punctuation:
      {
         // block
         if(token.mStart[0] == '{')
         {
            statement = parseStatementBlock(pContext);
         }
      }
      break;

      case TokenType::Keyword:
      {
         // usign namespace
         if(strncmp(token.mStart, "using", 5u) == 0)
         {
            tokenIndex++;
            const Token& nextToken = tokens[tokenIndex];

            if(strncmp(nextToken.mStart, "namespace", 9u) == 0)
            {
               tokenIndex++;
               Token& namespaceToken = const_cast<Token&>(pContext.mTokens[tokenIndex]);
               pContext.mStringBuffer.clear();

               do
               {
                  pContext.mStringBuffer.append(namespaceToken.mStart, namespaceToken.mLength);
                  tokenIndex++;
                  namespaceToken = tokens[tokenIndex];
               }
               while(*namespaceToken.mStart != ';');

               pContext.mUsingNamespaces.push_back(pContext.mStringBuffer);
            }

            break;
         }
         // if
         else if(strncmp(token.mStart, "if", 2u) == 0)
         {
            tokenIndex++;
            statement = parseStatementIf(pContext);
         }
         // while
         else if(strncmp(token.mStart, "while", 5u) == 0)
         {
            tokenIndex++;
            statement = parseStatementWhile(pContext);
         }
         // for
         else if(strncmp(token.mStart, "for", 3u) == 0)
         {
            tokenIndex++;
            statement = parseStatementFor(pContext);
         }
         // break
         else if(strncmp(token.mStart, "break", 5u) == 0)
         {
            tokenIndex++;
            statement = parseStatementBreak(pContext);
         }
         // continue
         else if(strncmp(token.mStart, "continue", 8u) == 0)
         {
            tokenIndex++;
            statement = parseStatementContinue(pContext);
         }
         // function declaration
         else if(strncmp(token.mStart, "void", 4u) == 0)
         {
            tokenIndex++;
            statement = parseStatementFunctionDeclaration(pContext);
         }
         // return
         else if(strncmp(token.mStart, "return", 6u) == 0)
         {
            tokenIndex++;
            statement = parseStatementReturn(pContext);
         }
      }
      break;

      case TokenType::Identifier:
      {
         // type
         TypeUsage typeUsage = parseTypeUsage(pContext);

         if(typeUsage.mType)
         {
            tokenIndex++;
            const Token& identifierToken = tokens[tokenIndex];
            pContext.mStringBuffer.assign(identifierToken.mStart, identifierToken.mLength);
            const Identifier identifier(pContext.mStringBuffer.c_str());

            tokenIndex++;
            const Token& nextToken = tokens[tokenIndex];

            if(nextToken.mType != TokenType::Operator && nextToken.mType != TokenType::Punctuation)
            {
               pContext.mStringBuffer.assign(token.mStart, token.mLength);
               throwCompileError(pContext, CompileError::UnexpectedSymbol, pContext.mStringBuffer.c_str());
               return nullptr;
            }

            // variable/const declaration
            if(nextToken.mStart[0] == '=' || nextToken.mStart[0] == ';')
            {
               Instance* existingInstance = retrieveInstance(pContext, identifier);

               if(!existingInstance)
               {
                  Expression* initialValue = nullptr;

                  if(nextToken.mStart[0] == '=')
                  {
                     tokenIndex++;
                     initialValue = parseExpression(pContext, findClosureTokenIndex(pContext, ' ', ';') - 1u);
                  }
                  else if(typeUsage.mType->mCategory != TypeCategory::BuiltIn && !typeUsage.isPointer())
                  {
                     Type* type = typeUsage.mType;
                     Method* defaultCtor = getDefaultConstructor(type);

                     if(!defaultCtor)
                     {
                        throwCompileError(pContext, CompileError::NoDefaultConstructor, type->mIdentifier.mName);
                        break;
                     }
                  }

                  registerInstance(pContext, typeUsage, identifier.mName);

                  statement = (StatementVariableDeclaration*)CflatMalloc(sizeof(StatementVariableDeclaration));
                  CflatInvokeCtor(StatementVariableDeclaration, statement)
                     (typeUsage, identifier, initialValue);
               }
               else
               {
                  throwCompileError(pContext, CompileError::VariableRedefinition, identifier.mName);
               }
            }
            // function declaration
            else if(nextToken.mStart[0] == '(')
            {
               tokenIndex--;
               statement = parseStatementFunctionDeclaration(pContext);
            }

            break;
         }
         // assignment/variable access/function call
         else
         {
            size_t cursor = tokenIndex;
            size_t operatorTokenIndex = 0u;
            uint32_t parenthesisLevel = 0u;

            while(cursor < tokens.size() && tokens[cursor++].mStart[0] != ';')
            {
               if(tokens[cursor].mType == TokenType::Operator && parenthesisLevel == 0u)
               {
                  bool isAssignmentOperator = false;

                  for(size_t i = 0u; i < kCflatAssignmentOperatorsCount; i++)
                  {
                     const size_t operatorStrLength = strlen(kCflatAssignmentOperators[i]);

                     if(tokens[cursor].mLength == operatorStrLength &&
                        strncmp(tokens[cursor].mStart, kCflatAssignmentOperators[i], operatorStrLength) == 0)
                     {
                        isAssignmentOperator = true;
                        break;
                     }
                  }

                  if(isAssignmentOperator)
                  {
                     operatorTokenIndex = cursor;
                     break;
                  }
               }

               if(tokens[cursor].mStart[0] == '(')
               {
                  parenthesisLevel++;
               }
               else if(tokens[cursor].mStart[0] == ')')
               {
                  parenthesisLevel--;
               }
            }

            // assignment
            if(operatorTokenIndex > 0u)
            {
               statement = parseStatementAssignment(pContext, operatorTokenIndex);
            }
            else
            {
               const Token& nextToken = tokens[tokenIndex + 1u];

               if(nextToken.mType == TokenType::Punctuation)
               {
                  // function call
                  if(nextToken.mStart[0] == '(')
                  {
                     pContext.mStringBuffer.assign(token.mStart, token.mLength);
                     Identifier identifier(pContext.mStringBuffer.c_str());

                     ExpressionFunctionCall* expression = 
                        (ExpressionFunctionCall*)CflatMalloc(sizeof(ExpressionFunctionCall));
                     CflatInvokeCtor(ExpressionFunctionCall, expression)(identifier);

                     tokenIndex++;
                     parseFunctionCallArguments(pContext, expression->mArguments);

                     statement = (StatementExpression*)CflatMalloc(sizeof(StatementExpression));
                     CflatInvokeCtor(StatementExpression, statement)(expression);
                  }
                  // member access
                  else
                  {
                     Expression* memberAccess =
                        parseExpression(pContext, findClosureTokenIndex(pContext, ' ', ';') - 1u);

                     if(memberAccess)
                     {
                        // method call
                        if(tokens[tokenIndex].mStart[0] == '(')
                        {
                           ExpressionMethodCall* expression = 
                              (ExpressionMethodCall*)CflatMalloc(sizeof(ExpressionMethodCall));
                           CflatInvokeCtor(ExpressionMethodCall, expression)(memberAccess);

                           parseFunctionCallArguments(pContext, expression->mArguments);

                           statement = (StatementExpression*)CflatMalloc(sizeof(StatementExpression));
                           CflatInvokeCtor(StatementExpression, statement)(expression);
                        }
                        // static method call
                        else
                        {
                           statement = (StatementExpression*)CflatMalloc(sizeof(StatementExpression));
                           CflatInvokeCtor(StatementExpression, statement)(memberAccess);
                        }
                     }
                  }
               }
               else if(nextToken.mType == TokenType::Operator)
               {
                  pContext.mStringBuffer.assign(token.mStart, token.mLength);
                  const char* variableName = pContext.mStringBuffer.c_str();
                  Instance* instance = retrieveInstance(pContext, variableName);

                  if(instance)
                  {
                     // increment
                     if(strncmp(nextToken.mStart, "++", 2u) == 0)
                     {
                        if(isInteger(*instance->mTypeUsage.mType))
                        {
                           statement = (StatementIncrement*)CflatMalloc(sizeof(StatementIncrement));
                           CflatInvokeCtor(StatementIncrement, statement)(variableName);
                           tokenIndex += 2u;
                        }
                        else
                        {
                           throwCompileError(pContext, CompileError::NonIntegerValue, variableName);
                        }
                     }
                     // decrement
                     else if(strncmp(nextToken.mStart, "--", 2u) == 0)
                     {
                        if(isInteger(*instance->mTypeUsage.mType))
                        {
                           statement = (StatementDecrement*)CflatMalloc(sizeof(StatementDecrement));
                           CflatInvokeCtor(StatementDecrement, statement)(variableName);
                           tokenIndex += 2u;
                        }
                        else
                        {
                           throwCompileError(pContext, CompileError::NonIntegerValue, variableName);
                        }
                     }
                  }
                  else
                  {
                     pContext.mStringBuffer.assign(token.mStart, token.mLength);
                     throwCompileError(pContext, CompileError::UndefinedVariable, pContext.mStringBuffer.c_str());
                  }
               }
               else
               {
                  pContext.mStringBuffer.assign(token.mStart, token.mLength);
                  throwCompileError(pContext, CompileError::UnexpectedSymbol, pContext.mStringBuffer.c_str());
               }
            }
         }
      }
      break;
   }

   if(statement)
   {
      statement->mLine = statementLine;
   }

   return statement;
}

StatementBlock* Environment::parseStatementBlock(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;
   const Token& token = tokens[tokenIndex];

   if(token.mStart[0] != '{')
      return nullptr;

   StatementBlock* block = (StatementBlock*)CflatMalloc(sizeof(StatementBlock));
   CflatInvokeCtor(StatementBlock, block)();

   incrementScopeLevel(pContext);

   while(tokens[tokenIndex].mStart[0] != '}')
   {
      tokenIndex++;
      Statement* statement = parseStatement(pContext);

      if(statement)
      {
         block->mStatements.push_back(statement);
      }
   }

   decrementScopeLevel(pContext);

   return block;
}

StatementFunctionDeclaration* Environment::parseStatementFunctionDeclaration(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;
   const Token& token = tokens[tokenIndex];
   const Token& previousToken = tokens[tokenIndex - 1u];

   pContext.mStringBuffer.assign(previousToken.mStart, previousToken.mLength);
   TypeUsage returnType = getTypeUsage(pContext.mStringBuffer.c_str());

   pContext.mStringBuffer.assign(token.mStart, token.mLength);
   const Identifier functionIdentifier(pContext.mStringBuffer.c_str());

   StatementFunctionDeclaration* statement =
      (StatementFunctionDeclaration*)CflatMalloc(sizeof(StatementFunctionDeclaration));
   CflatInvokeCtor(StatementFunctionDeclaration, statement)(returnType, functionIdentifier);

   tokenIndex++;

   while(tokens[tokenIndex++].mStart[0] != ')')
   {
      // no arguments
      if(tokens[tokenIndex].mStart[0] == ')')
      {
         tokenIndex++;
         break;
      }

      TypeUsage parameterType = parseTypeUsage(pContext);
      statement->mParameterTypes.push_back(parameterType);
      tokenIndex++;

      pContext.mStringBuffer.assign(tokens[tokenIndex].mStart, tokens[tokenIndex].mLength);
      Identifier parameterIdentifier(pContext.mStringBuffer.c_str());
      statement->mParameterIdentifiers.push_back(parameterIdentifier);
      tokenIndex++;

      Instance* parameterInstance =
         registerInstance(pContext, parameterType, parameterIdentifier.mName);
      parameterInstance->mScopeLevel++;
   }

   statement->mBody = parseStatementBlock(pContext);

   return statement;
}

StatementAssignment* Environment::parseStatementAssignment(ParsingContext& pContext, size_t pOperatorTokenIndex)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   Expression* leftValue = parseExpression(pContext, pOperatorTokenIndex - 1u);

   const Token& operatorToken = pContext.mTokens[pOperatorTokenIndex];
   CflatSTLString operatorStr(operatorToken.mStart, operatorToken.mLength);

   tokenIndex = pOperatorTokenIndex + 1u;
   Expression* rightValue = parseExpression(pContext, findClosureTokenIndex(pContext, ' ', ';') - 1u);

   StatementAssignment* statement = (StatementAssignment*)CflatMalloc(sizeof(StatementAssignment));
   CflatInvokeCtor(StatementAssignment, statement)(leftValue, rightValue, operatorStr.c_str());

   return statement;
}

StatementIf* Environment::parseStatementIf(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   if(tokens[tokenIndex].mStart[0] != '(')
   {
      return nullptr;
   }

   tokenIndex++;
   const size_t conditionClosureTokenIndex = findClosureTokenIndex(pContext, '(', ')');
   Expression* condition = parseExpression(pContext, conditionClosureTokenIndex - 1u);
   tokenIndex = conditionClosureTokenIndex + 1u;

   Statement* ifStatement = parseStatement(pContext);
   tokenIndex++;

   Statement* elseStatement = nullptr;

   if(tokens[tokenIndex].mType == TokenType::Keyword &&
      strncmp(tokens[tokenIndex].mStart, "else", 4u) == 0)
   {
      tokenIndex++;
      elseStatement = parseStatement(pContext);
   }

   StatementIf* statement = (StatementIf*)CflatMalloc(sizeof(StatementIf));
   CflatInvokeCtor(StatementIf, statement)(condition, ifStatement, elseStatement);

   return statement;
}

StatementWhile* Environment::parseStatementWhile(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   if(tokens[tokenIndex].mStart[0] != '(')
   {
      return nullptr;
   }

   tokenIndex++;
   const size_t conditionClosureTokenIndex = findClosureTokenIndex(pContext, '(', ')');
   Expression* condition = parseExpression(pContext, conditionClosureTokenIndex - 1u);
   tokenIndex = conditionClosureTokenIndex + 1u;

   Statement* loopStatement = parseStatement(pContext);

   StatementWhile* statement = (StatementWhile*)CflatMalloc(sizeof(StatementWhile));
   CflatInvokeCtor(StatementWhile, statement)(condition, loopStatement);

   return statement;
}

StatementFor* Environment::parseStatementFor(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   if(tokens[tokenIndex].mStart[0] != '(')
   {
      return nullptr;
   }

   incrementScopeLevel(pContext);

   tokenIndex++;
   const size_t initializationClosureTokenIndex = findClosureTokenIndex(pContext, ' ', ';');
   Statement* initialization = parseStatement(pContext);
   tokenIndex = initializationClosureTokenIndex + 1u;

   const size_t conditionClosureTokenIndex = findClosureTokenIndex(pContext, ' ', ';');
   Expression* condition = parseExpression(pContext, conditionClosureTokenIndex - 1u);
   tokenIndex = conditionClosureTokenIndex + 1u;

   const size_t incrementClosureTokenIndex = findClosureTokenIndex(pContext, '(', ')');
   Statement* increment = parseStatement(pContext);
   tokenIndex = incrementClosureTokenIndex + 1u;

   Statement* loopStatement = parseStatement(pContext);

   decrementScopeLevel(pContext);

   StatementFor* statement = (StatementFor*)CflatMalloc(sizeof(StatementFor));
   CflatInvokeCtor(StatementFor, statement)(initialization, condition, increment, loopStatement);

   return statement;
}

StatementBreak* Environment::parseStatementBreak(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   if(tokens[tokenIndex].mStart[0] != ';')
   {
      throwCompileError(pContext, CompileError::UnexpectedSymbol, "break");
      return nullptr;
   }

   StatementBreak* statement = (StatementBreak*)CflatMalloc(sizeof(StatementBreak));
   CflatInvokeCtor(StatementBreak, statement)();

   return statement;
}

StatementContinue* Environment::parseStatementContinue(ParsingContext& pContext)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   if(tokens[tokenIndex].mStart[0] != ';')
   {
      throwCompileError(pContext, CompileError::UnexpectedSymbol, "continue");
      return nullptr;
   }

   StatementContinue* statement = (StatementContinue*)CflatMalloc(sizeof(StatementContinue));
   CflatInvokeCtor(StatementContinue, statement)();

   return statement;
}

StatementReturn* Environment::parseStatementReturn(ParsingContext& pContext)
{
   Expression* expression = parseExpression(pContext, findClosureTokenIndex(pContext, ' ', ';') - 1u);

   StatementReturn* statement = (StatementReturn*)CflatMalloc(sizeof(StatementReturn));
   CflatInvokeCtor(StatementReturn, statement)(expression);

   return statement;
}

bool Environment::parseFunctionCallArguments(ParsingContext& pContext, CflatSTLVector<Expression*>& pArguments)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   while(tokens[tokenIndex++].mStart[0] != ')')
   {
      const size_t closureTokenIndex = findClosureTokenIndex(pContext, ' ', ')');
      const size_t separatorTokenIndex = findClosureTokenIndex(pContext, ' ', ',');

      size_t tokenLastIndex = closureTokenIndex;

      if(separatorTokenIndex > 0u && separatorTokenIndex < closureTokenIndex)
      {
         tokenLastIndex = separatorTokenIndex;
      }

      Expression* argument = parseExpression(pContext, tokenLastIndex - 1u);

      if(argument)
      {
         pArguments.push_back(argument);
         tokenIndex++;
      }
   }

   return true;
}

bool Environment::parseMemberAccessIdentifiers(ParsingContext& pContext, CflatSTLVector<Identifier>& pIdentifiers)
{
   CflatSTLVector<Token>& tokens = pContext.mTokens;
   size_t& tokenIndex = pContext.mTokenIndex;

   TypeUsage typeUsage;
   bool anyRemainingMemberAccessIdentifiers = true;

   while(anyRemainingMemberAccessIdentifiers)
   {
      const bool memberAccess = tokens[tokenIndex + 1u].mStart[0] == '.';
      const bool ptrMemberAccess = !memberAccess && strncmp(tokens[tokenIndex + 1u].mStart, "->", 2u) == 0;

      anyRemainingMemberAccessIdentifiers = memberAccess || ptrMemberAccess;

      pContext.mStringBuffer.assign(tokens[tokenIndex].mStart, tokens[tokenIndex].mLength);
      pIdentifiers.push_back(pContext.mStringBuffer.c_str());

      if(pIdentifiers.size() == 1u)
      {
         Instance* instance = retrieveInstance(pContext, pIdentifiers.back());
         typeUsage = instance->mValue.mTypeUsage;
      }
      else if(tokens[tokenIndex + 1u].mStart[0] != '(')
      {
         const char* memberName = pIdentifiers.back().mName;
         Struct* type = static_cast<Struct*>(typeUsage.mType);
         Member* member = nullptr;

         for(size_t j = 0u; j < type->mMembers.size(); j++)
         {
            if(strcmp(type->mMembers[j].mIdentifier.mName, memberName) == 0)
            {
               member = &type->mMembers[j];
               break;
            }
         }

         if(member)
         {
            typeUsage = member->mTypeUsage;
         }
         else
         {
            throwCompileError(pContext, CompileError::MissingMember, memberName);
            return false;
         }
      }
      else
      {
         // method call
         typeUsage = TypeUsage();
      }

      if(typeUsage.isPointer())
      {
         if(!ptrMemberAccess)
         {
            throwCompileError(pContext, CompileError::InvalidMemberAccessOperatorPtr, pIdentifiers.back().mName);
            return false;
         }
      }
      else
      {
         if(ptrMemberAccess)
         {
            throwCompileError(pContext, CompileError::InvalidMemberAccessOperatorNonPtr, pIdentifiers.back().mName);
            return false;
         }
      }

      tokenIndex++;

      if(anyRemainingMemberAccessIdentifiers)
      {
         tokenIndex++;
      }
   }

   return true;
}

Instance* Environment::registerInstance(Context& pContext,
   const TypeUsage& pTypeUsage, const Identifier& pIdentifier)
{
   //TODO: register the instance in the current namespace
   Instance* instance = mGlobalNamespace.registerInstance(pTypeUsage, pIdentifier);
   instance->mScopeLevel = pContext.mScopeLevel;

   if(instance->mTypeUsage.isReference())
   {
      instance->mValue.initExternal(instance->mTypeUsage);
   }
   else
   {
      instance->mValue.initOnStack(instance->mTypeUsage, &pContext.mStack);
   }

   return instance;
}

Instance* Environment::retrieveInstance(const Context& pContext, const Identifier& pIdentifier)
{
   //TODO: retrieve the instance from the current namespace or any of the usings
   return mGlobalNamespace.retrieveInstance(pIdentifier);
}

void Environment::incrementScopeLevel(Context& pContext)
{
   pContext.mScopeLevel++;
}

void Environment::decrementScopeLevel(Context& pContext)
{
   mGlobalNamespace.releaseInstances(pContext.mScopeLevel);
   pContext.mScopeLevel--;
}

void Environment::throwRuntimeError(ExecutionContext& pContext, RuntimeError pError, const char* pArg)
{
   char errorMsg[256];
   sprintf(errorMsg, kRuntimeErrorStrings[(int)pError], pArg);

   pContext.mErrorMessage.assign("[Runtime Error] Line ");
   pContext.mErrorMessage.append(std::to_string(pContext.mCurrentLine));
   pContext.mErrorMessage.append(": ");
   pContext.mErrorMessage.append(errorMsg);
}

void Environment::getValue(ExecutionContext& pContext, Expression* pExpression, Value* pOutValue)
{
   switch(pExpression->getType())
   {
   case ExpressionType::Value:
      {
         ExpressionValue* expression = static_cast<ExpressionValue*>(pExpression);
         *pOutValue = expression->mValue;
      }
      break;
   case ExpressionType::NullPointer:
      {
         void* nullPointer = nullptr;
         pOutValue->set(&nullPointer);
      }
      break;
   case ExpressionType::VariableAccess:
      {
         ExpressionVariableAccess* expression = static_cast<ExpressionVariableAccess*>(pExpression);
         Instance* instance = retrieveInstance(pContext, expression->mVariableIdentifier);
         *pOutValue = instance->mValue;
      }
      break;
   case ExpressionType::BinaryOperation:
      {
         ExpressionBinaryOperation* expression = static_cast<ExpressionBinaryOperation*>(pExpression);

         Value leftValue;
         getValue(pContext, expression->mLeft, &leftValue);
         Value rightValue;
         getValue(pContext, expression->mRight, &rightValue);

         applyBinaryOperator(pContext, leftValue, rightValue, expression->mOperator, pOutValue);
      }
      break;
   case ExpressionType::Parenthesized:
      {
         ExpressionParenthesized* expression = static_cast<ExpressionParenthesized*>(pExpression);
         getValue(pContext, expression->mExpression, pOutValue);
      }
      break;
   case ExpressionType::AddressOf:
      {
         ExpressionAddressOf* expression = static_cast<ExpressionAddressOf*>(pExpression);

         if(expression->mExpression->getType() == ExpressionType::VariableAccess)
         {
            ExpressionVariableAccess* variableAccess =
               static_cast<ExpressionVariableAccess*>(expression->mExpression);
            Instance* instance = retrieveInstance(pContext, variableAccess->mVariableIdentifier);
            getAddressOfValue(pContext, &instance->mValue, pOutValue);
         }
      }
      break;
   case ExpressionType::FunctionCall:
      {
         ExpressionFunctionCall* expression = static_cast<ExpressionFunctionCall*>(pExpression);
         Function* function = getFunction(expression->mFunctionIdentifier);

         CflatSTLVector<Value> argumentValues;
         getArgumentValues(pContext, function->mParameters, expression->mArguments, argumentValues);

         const bool functionReturnValueIsConst =
            CflatHasFlag(function->mReturnTypeUsage.mFlags, TypeUsageFlags::Const);
         const bool outValueIsConst =
            CflatHasFlag(pOutValue->mTypeUsage.mFlags, TypeUsageFlags::Const);

         if(outValueIsConst && !functionReturnValueIsConst)
         {
            CflatResetFlag(pOutValue->mTypeUsage.mFlags, TypeUsageFlags::Const);
         }

         function->execute(argumentValues, pOutValue);

         if(outValueIsConst && !functionReturnValueIsConst)
         {
            CflatSetFlag(pOutValue->mTypeUsage.mFlags, TypeUsageFlags::Const);
         }
      }
      break;
   case ExpressionType::MethodCall:
      {
         ExpressionMethodCall* expression = static_cast<ExpressionMethodCall*>(pExpression);
         ExpressionMemberAccess* memberAccess = static_cast<ExpressionMemberAccess*>(expression->mMemberAccess);

         Value instanceDataValue;
         getInstanceDataValue(pContext, memberAccess, &instanceDataValue);

         if(!pContext.mErrorMessage.empty())
            break;

         const Identifier& methodIdentifier = memberAccess->mIdentifiers.back();
         Method* method = findMethod(instanceDataValue.mTypeUsage.mType, methodIdentifier);
         CflatAssert(method);

         Value thisPtr;

         if(instanceDataValue.mTypeUsage.isPointer())
         {
            thisPtr.initOnStack(instanceDataValue.mTypeUsage, &pContext.mStack);
            thisPtr.set(instanceDataValue.mValueBuffer);
         }
         else
         {
            getAddressOfValue(pContext, &instanceDataValue, &thisPtr);
         }

         pContext.mReturnValue = Value();
         pContext.mReturnValue.initOnHeap(method->mReturnTypeUsage);

         CflatSTLVector<Value> argumentValues;
         getArgumentValues(pContext, method->mParameters, expression->mArguments, argumentValues);

         method->execute(thisPtr, argumentValues, &pContext.mReturnValue);
      }
      break;
   default:
      break;
   }
}

void Environment::getInstanceDataValue(ExecutionContext& pContext, Expression* pExpression, Value* pOutValue)
{
   if(pExpression->getType() == ExpressionType::VariableAccess)
   {
      ExpressionVariableAccess* variableAccess = static_cast<ExpressionVariableAccess*>(pExpression);
      Instance* instance = retrieveInstance(pContext, variableAccess->mVariableIdentifier);
      *pOutValue = instance->mValue;
   }
   else if(pExpression->getType() == ExpressionType::MemberAccess)
   {
      ExpressionMemberAccess* memberAccess = static_cast<ExpressionMemberAccess*>(pExpression);

      const Identifier& instanceIdentifier = memberAccess->mIdentifiers[0];
      Instance* instance = retrieveInstance(pContext, instanceIdentifier);
      *pOutValue = instance->mValue;

      if(pOutValue->mTypeUsage.isPointer() && !CflatRetrieveValue(pOutValue, void*))
      {
         throwRuntimeError(pContext, RuntimeError::NullPointerAccess, instanceIdentifier.mName);
         return;
      }

      for(size_t i = 1u; i < memberAccess->mIdentifiers.size(); i++)
      {
         const Identifier& memberIdentifier = memberAccess->mIdentifiers[i];
         Struct* type = static_cast<Struct*>(pOutValue->mTypeUsage.mType);
         Member* member = nullptr;

         for(size_t j = 0u; j < type->mMembers.size(); j++)
         {
            if(type->mMembers[j].mIdentifier == memberIdentifier)
            {
               member = &type->mMembers[j];
               break;
            }
         }

         // the symbol is a member
         if(member)
         {
            char* instanceDataPtr = pOutValue->mTypeUsage.isPointer()
               ? CflatRetrieveValue(pOutValue, char*)
               : pOutValue->mValueBuffer;

            pOutValue->mTypeUsage = member->mTypeUsage;
            pOutValue->mValueBuffer = instanceDataPtr + member->mOffset;

            if(pOutValue->mTypeUsage.isPointer() && !CflatRetrieveValue(pOutValue, void*))
            {
               throwRuntimeError(pContext, RuntimeError::NullPointerAccess, member->mIdentifier.mName);
               break;
            }
         }
         // the symbol is a method
         else
         {
            break;
         }
      }
   }
}

void Environment::getAddressOfValue(ExecutionContext& pContext, Value* pInstanceDataValue, Value* pOutValue)
{
   TypeUsage pointerTypeUsage = pInstanceDataValue->mTypeUsage;
   pointerTypeUsage.mPointerLevel++;

   assertValueInitialization(pointerTypeUsage, pOutValue);
   pOutValue->set(&pInstanceDataValue->mValueBuffer);
}

void Environment::getArgumentValues(ExecutionContext& pContext, const CflatSTLVector<TypeUsage>& pParameters,
   const CflatSTLVector<Expression*>& pExpressions, CflatSTLVector<Value>& pValues)
{
   CflatAssert(pParameters.size() == pExpressions.size());
   pValues.resize(pExpressions.size());

   for(size_t i = 0u; i < pExpressions.size(); i++)
   {
      getValue(pContext, pExpressions[i], &pValues[i]);

      if(pParameters[i].isReference())
      {
         // pass by reference
         if(pValues[i].mValueBufferType != ValueBufferType::External)
         {
            TypeUsage cachedTypeUsage = pValues[i].mTypeUsage;
            char* cachedValueBuffer = pValues[i].mValueBuffer;
            pValues[i].reset();

            pValues[i].initExternal(cachedTypeUsage);
            pValues[i].set(cachedValueBuffer);
         }

         CflatSetFlag(pValues[i].mTypeUsage.mFlags, TypeUsageFlags::Reference);
      }
      else
      {
         // pass by value
         if(pValues[i].mValueBufferType == ValueBufferType::External)
         {
            TypeUsage cachedTypeUsage = pValues[i].mTypeUsage;
            char* cachedValueBuffer = pValues[i].mValueBuffer;
            pValues[i].reset();

            pValues[i].initOnHeap(cachedTypeUsage);
            pValues[i].set(cachedValueBuffer);
         }
      }
   }
}

void Environment::applyBinaryOperator(ExecutionContext& pContext, const Value& pLeft, const Value& pRight,
   const char* pOperator, Value* pOutValue)
{
   Type* leftType = pLeft.mTypeUsage.mType;

   if(leftType->mCategory == TypeCategory::BuiltIn)
   {
      const bool integerValues = isInteger(*leftType);

      const int64_t leftValueAsInteger = getValueAsInteger(pLeft);
      const int64_t rightValueAsInteger = getValueAsInteger(pRight);
      const double leftValueAsDecimal = getValueAsDecimal(pLeft);
      const double rightValueAsDecimal = getValueAsDecimal(pRight);

      if(strcmp(pOperator, "==") == 0)
      {
         const bool result = leftValueAsInteger == rightValueAsInteger;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, "!=") == 0)
      {
         const bool result = leftValueAsInteger != rightValueAsInteger;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, "<") == 0)
      {
         const bool result = integerValues
            ? leftValueAsInteger < rightValueAsInteger
            : leftValueAsDecimal < rightValueAsDecimal;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, ">") == 0)
      {
         const bool result = integerValues
            ? leftValueAsInteger > rightValueAsInteger
            : leftValueAsDecimal > rightValueAsDecimal;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, "<=") == 0)
      {
         const bool result = integerValues
            ? leftValueAsInteger <= rightValueAsInteger
            : leftValueAsDecimal <= rightValueAsDecimal;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, ">=") == 0)
      {
         const bool result = integerValues
            ? leftValueAsInteger >= rightValueAsInteger
            : leftValueAsDecimal >= rightValueAsDecimal;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, "&&") == 0)
      {
         const bool result = leftValueAsInteger && rightValueAsInteger;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, "||") == 0)
      {
         const bool result = leftValueAsInteger || rightValueAsInteger;

         const TypeUsage typeUsage = getTypeUsage("bool");
         assertValueInitialization(typeUsage, pOutValue);
         pOutValue->set(&result);
      }
      else if(strcmp(pOperator, "+") == 0)
      {
         assertValueInitialization(pLeft.mTypeUsage, pOutValue);

         if(integerValues)
         {
            setValueAsInteger(leftValueAsInteger + rightValueAsInteger, pOutValue);
         }
         else
         {
            setValueAsDecimal(leftValueAsDecimal + rightValueAsDecimal, pOutValue);
         }
      }
      else if(strcmp(pOperator, "-") == 0)
      {
         assertValueInitialization(pLeft.mTypeUsage, pOutValue);

         if(integerValues)
         {
            setValueAsInteger(leftValueAsInteger - rightValueAsInteger, pOutValue);
         }
         else
         {
            setValueAsDecimal(leftValueAsDecimal - rightValueAsDecimal, pOutValue);
         }
      }
      else if(strcmp(pOperator, "*") == 0)
      {
         assertValueInitialization(pLeft.mTypeUsage, pOutValue);

         if(integerValues)
         {
            setValueAsInteger(leftValueAsInteger * rightValueAsInteger, pOutValue);
         }
         else
         {
            setValueAsDecimal(leftValueAsDecimal * rightValueAsDecimal, pOutValue);
         }
      }
      else if(strcmp(pOperator, "/") == 0)
      {
         assertValueInitialization(pLeft.mTypeUsage, pOutValue);

         if(integerValues)
         {
            if(rightValueAsInteger != 0)
            {
               setValueAsInteger(leftValueAsInteger / rightValueAsInteger, pOutValue);
            }
            else
            {
               throwRuntimeError(pContext, RuntimeError::DivisionByZero);
            }
         }
         else
         {
            if(fabs(rightValueAsDecimal) > 0.000000001)
            {
               setValueAsDecimal(leftValueAsDecimal / rightValueAsDecimal, pOutValue);
            }
            else
            {
               throwRuntimeError(pContext, RuntimeError::DivisionByZero);
            }
         }
      }
   }
   else
   {
      pContext.mStringBuffer.assign("operator");
      pContext.mStringBuffer.append(pOperator);

      Method* operatorMethod = findMethod(leftType, pContext.mStringBuffer.c_str());
      CflatAssert(operatorMethod);

      Value thisPtrValue;
      getAddressOfValue(pContext, &const_cast<Cflat::Value&>(pLeft), &thisPtrValue);

      assertValueInitialization(operatorMethod->mReturnTypeUsage, pOutValue);

      CflatSTLVector<Value> args;
      args.push_back(pRight);

      operatorMethod->execute(thisPtrValue, args, pOutValue);
   }
}

void Environment::performAssignment(ExecutionContext& pContext, Value* pValue,
   const char* pOperator, Value* pInstanceDataValue)
{
   if(strcmp(pOperator, "=") == 0)
   {
      memcpy(pInstanceDataValue->mValueBuffer, pValue->mValueBuffer, pValue->mTypeUsage.getSize());
   }
}

void Environment::execute(ExecutionContext& pContext, const Program& pProgram)
{
   for(size_t i = 0u; i < pProgram.mStatements.size(); i++)
   {
      execute(pContext, pProgram.mStatements[i]);

      if(!pContext.mErrorMessage.empty())
         break;
   }
}

void Environment::assertValueInitialization(const TypeUsage& pTypeUsage, Value* pOutValue)
{
   if(pOutValue->mValueBufferType == ValueBufferType::Uninitialized ||
      !pOutValue->mTypeUsage.compatibleWith(pTypeUsage))
   {
      pOutValue->initOnHeap(pTypeUsage);
   }
}

bool Environment::isInteger(const Type& pType)
{
   return pType.mCategory == TypeCategory::BuiltIn && !isDecimal(pType);
}

bool Environment::isDecimal(const Type& pType)
{
   return pType.mCategory == TypeCategory::BuiltIn &&
      (strncmp(pType.mIdentifier.mName, "float", 5u) == 0 ||
       strcmp(pType.mIdentifier.mName, "double") == 0);
}

int64_t Environment::getValueAsInteger(const Value& pValue)
{
   int64_t valueAsInteger = 0u;

   if(pValue.mTypeUsage.mType->mSize == 4u)
   {
      valueAsInteger = (int64_t)CflatRetrieveValue(&pValue, int32_t);
   }
   else if(pValue.mTypeUsage.mType->mSize == 8u)
   {
      valueAsInteger = CflatRetrieveValue(&pValue, int64_t);
   }
   else if(pValue.mTypeUsage.mType->mSize == 2u)
   {
      valueAsInteger = (int64_t)CflatRetrieveValue(&pValue, int16_t);
   }
   else if(pValue.mTypeUsage.mType->mSize == 1u)
   {
      valueAsInteger = (int64_t)CflatRetrieveValue(&pValue, int8_t);
   }

   return valueAsInteger;
}

double Environment::getValueAsDecimal(const Value& pValue)
{
   double valueAsDecimal = 0.0;

   if(pValue.mTypeUsage.mType->mSize == 4u)
   {
      valueAsDecimal = (double)CflatRetrieveValue(&pValue, float);
   }
   else if(pValue.mTypeUsage.mType->mSize == 8u)
   {
      valueAsDecimal = CflatRetrieveValue(&pValue, double);
   }

   return valueAsDecimal;
}

void Environment::setValueAsInteger(int64_t pInteger, Value* pOutValue)
{
   const size_t typeSize = pOutValue->mTypeUsage.mType->mSize;

   if(typeSize == 4u)
   {
      const int32_t value = (int32_t)pInteger;
      pOutValue->set(&value);
   }
   else if(typeSize == 8u)
   {
      pOutValue->set(&pInteger);
   }
   else if(typeSize == 2u)
   {
      const int16_t value = (int16_t)pInteger;
      pOutValue->set(&value);
   }
   else if(typeSize == 1u)
   {
      const int8_t value = (int8_t)pInteger;
      pOutValue->set(&value);
   }
}

void Environment::setValueAsDecimal(double pDecimal, Value* pOutValue)
{
   const size_t typeSize = pOutValue->mTypeUsage.mType->mSize;

   if(typeSize == 4u)
   {
      const float value = (float)pDecimal;
      pOutValue->set(&value);
   }
   else if(typeSize == 8u)
   {
      pOutValue->set(&pDecimal);
   }
}

Method* Environment::getDefaultConstructor(Type* pType)
{
   CflatAssert(pType->mCategory != TypeCategory::BuiltIn);

   Method* defaultConstructor = nullptr;
   Struct* type = static_cast<Struct*>(pType);

   for(size_t i = 0u; i < type->mMethods.size(); i++)
   {
      if(type->mMethods[i].mParameters.empty() &&
         type->mMethods[i].mIdentifier == type->mIdentifier)
      {
         defaultConstructor = &type->mMethods[i];
         break;
      }
   }

   return defaultConstructor;
}

Method* Environment::findMethod(Type* pType, const Identifier& pIdentifier)
{
   CflatAssert(pType->mCategory != TypeCategory::BuiltIn);

   Method* method = nullptr;
   Struct* type = static_cast<Struct*>(pType);

   for(size_t i = 0u; i < type->mMethods.size(); i++)
   {
      if(type->mMethods[i].mIdentifier == pIdentifier)
      {
         method = &type->mMethods[i];
         break;
      }
   }

   return method;
}

void Environment::execute(ExecutionContext& pContext, Statement* pStatement)
{
   pContext.mCurrentLine = pStatement->mLine;

   switch(pStatement->getType())
   {
   case StatementType::Expression:
      {
         StatementExpression* statement = static_cast<StatementExpression*>(pStatement);

         Value unusedValue;
         getValue(pContext, statement->mExpression, &unusedValue);
      }
      break;
   case StatementType::Block:
      {
         incrementScopeLevel(pContext);

         StatementBlock* statement = static_cast<StatementBlock*>(pStatement);

         for(size_t i = 0u; i < statement->mStatements.size(); i++)
         {
            execute(pContext, statement->mStatements[i]);

            if(pContext.mJumpStatement != JumpStatement::None)
            {
               break;
            }
         }

         decrementScopeLevel(pContext);
      }
      break;
   case StatementType::UsingDirective:
      {
      }
      break;
   case StatementType::NamespaceDeclaration:
      {
      }
      break;
   case StatementType::VariableDeclaration:
      {
         StatementVariableDeclaration* statement = static_cast<StatementVariableDeclaration*>(pStatement);
         Instance* instance = registerInstance(pContext, statement->mTypeUsage, statement->mVariableIdentifier);

         // if there is an assignment in the declaration, set the value
         if(statement->mInitialValue)
         {
            getValue(pContext, statement->mInitialValue, &instance->mValue);
         }
         // otherwise, call the default constructor if the type is a struct or a class
         else if(instance->mTypeUsage.mType->mCategory != TypeCategory::BuiltIn &&
            !instance->mTypeUsage.isPointer())
         {
            instance->mValue.mTypeUsage = instance->mTypeUsage;
            Value thisPtr;
            getAddressOfValue(pContext, &instance->mValue, &thisPtr);

            Method* defaultCtor = getDefaultConstructor(instance->mTypeUsage.mType);
            CflatSTLVector<Value> args;
            defaultCtor->execute(thisPtr, args, nullptr);
         }
      }
      break;
   case StatementType::FunctionDeclaration:
      {
         StatementFunctionDeclaration* statement = static_cast<StatementFunctionDeclaration*>(pStatement);
         Function* function = registerFunction(statement->mFunctionIdentifier);
         function->mReturnTypeUsage = statement->mReturnType;

         for(size_t i = 0u; i < statement->mParameterTypes.size(); i++)
         {
            function->mParameters.push_back(statement->mParameterTypes[i]);
         }

         if(statement->mBody)
         {
            function->execute =
               [this, &pContext, function, statement](CflatSTLVector<Value>& pArguments, Value* pOutReturnValue)
            {
               CflatAssert(function->mParameters.size() == pArguments.size());
               
               for(size_t i = 0u; i < pArguments.size(); i++)
               {
                  const TypeUsage parameterType = statement->mParameterTypes[i];
                  const Identifier& parameterIdentifier = statement->mParameterIdentifiers[i];
                  Instance* argumentInstance = registerInstance(pContext, parameterType, parameterIdentifier);
                  argumentInstance->mScopeLevel++;
                  argumentInstance->mValue.set(pArguments[i].mValueBuffer);
               }

               execute(pContext, statement->mBody);

               if(function->mReturnTypeUsage.mType && pOutReturnValue)
               {
                  assertValueInitialization(pContext.mReturnValue.mTypeUsage, pOutReturnValue);
                  pOutReturnValue->set(pContext.mReturnValue.mValueBuffer);
               }

               pContext.mJumpStatement = JumpStatement::None;
            };
         }
      }
      break;
   case StatementType::Assignment:
      {
         StatementAssignment* statement = static_cast<StatementAssignment*>(pStatement);

         Value instanceDataValue;
         getInstanceDataValue(pContext, statement->mLeftValue, &instanceDataValue);

         Value rightValue;
         getValue(pContext, statement->mRightValue, &rightValue);

         performAssignment(pContext, &rightValue, statement->mOperator, &instanceDataValue);
      }
      break;
   case StatementType::Increment:
      {
         StatementIncrement* statement = static_cast<StatementIncrement*>(pStatement);
         Instance* instance = retrieveInstance(pContext, statement->mVariableIdentifier);
         CflatAssert(instance);
         setValueAsInteger(getValueAsInteger(instance->mValue) + 1u, &instance->mValue);
      }
      break;
   case StatementType::Decrement:
      {
         StatementDecrement* statement = static_cast<StatementDecrement*>(pStatement);
         Instance* instance = retrieveInstance(pContext, statement->mVariableIdentifier);
         CflatAssert(instance);
         setValueAsInteger(getValueAsInteger(instance->mValue) - 1u, &instance->mValue);
      }
      break;
   case StatementType::If:
      {
         StatementIf* statement = static_cast<StatementIf*>(pStatement);

         Value conditionValue;
         getValue(pContext, statement->mCondition, &conditionValue);
         const bool conditionMet = CflatRetrieveValue(&conditionValue, bool);

         if(conditionMet)
         {
            execute(pContext, statement->mIfStatement);
         }
         else if(statement->mElseStatement)
         {
            execute(pContext, statement->mElseStatement);
         }
      }
      break;
   case StatementType::While:
      {
         StatementWhile* statement = static_cast<StatementWhile*>(pStatement);

         Value conditionValue;
         getValue(pContext, statement->mCondition, &conditionValue);
         bool conditionMet = CflatRetrieveValue(&conditionValue, bool);

         while(conditionMet)
         {
            if(pContext.mJumpStatement == JumpStatement::Continue)
            {
               pContext.mJumpStatement = JumpStatement::None;
            }

            execute(pContext, statement->mLoopStatement);

            if(pContext.mJumpStatement == JumpStatement::Break)
            {
               pContext.mJumpStatement = JumpStatement::None;
               break;
            }

            getValue(pContext, statement->mCondition, &conditionValue);
            conditionMet = CflatRetrieveValue(&conditionValue, bool);
         }
      }
      break;
   case StatementType::For:
      {
         StatementFor* statement = static_cast<StatementFor*>(pStatement);

         incrementScopeLevel(pContext);

         if(statement->mInitialization)
         {
            execute(pContext, statement->mInitialization);
         }

         const bool defaultConditionValue = true;

         Value conditionValue;
         conditionValue.initOnHeap(getTypeUsage("bool"));
         conditionValue.set(&defaultConditionValue);

         bool conditionMet = defaultConditionValue;

         if(statement->mCondition)
         {
            getValue(pContext, statement->mCondition, &conditionValue);
            conditionMet = CflatRetrieveValue(&conditionValue, bool);
         }

         while(conditionMet)
         {
            if(pContext.mJumpStatement == JumpStatement::Continue)
            {
               pContext.mJumpStatement = JumpStatement::None;
            }

            execute(pContext, statement->mLoopStatement);

            if(pContext.mJumpStatement == JumpStatement::Break)
            {
               pContext.mJumpStatement = JumpStatement::None;
               break;
            }

            if(statement->mIncrement)
            {
               execute(pContext, statement->mIncrement);
            }

            if(statement->mCondition)
            {
               getValue(pContext, statement->mCondition, &conditionValue);
               conditionMet = CflatRetrieveValue(&conditionValue, bool);
            }
         }

         decrementScopeLevel(pContext);
      }
      break;
   case StatementType::Break:
      {
         pContext.mJumpStatement = JumpStatement::Break;
      }
      break;
   case StatementType::Continue:
      {
         pContext.mJumpStatement = JumpStatement::Continue;
      }
      break;
   case StatementType::Return:
      {
         StatementReturn* statement = static_cast<StatementReturn*>(pStatement);

         if(statement->mExpression)
         {
            getValue(pContext, statement->mExpression, &pContext.mReturnValue);
         }

         pContext.mJumpStatement = JumpStatement::Return;
      }
      break;
   default:
      break;
   }
}

TypeUsage Environment::getTypeUsage(const char* pTypeName)
{
   TypeUsage typeUsage;

   const size_t typeNameLength = strlen(pTypeName);
   const char* baseTypeNameStart = pTypeName;
   const char* baseTypeNameEnd = pTypeName + typeNameLength - 1u;

   // is it const?
   const char* typeNameConst = strstr(pTypeName, "const");

   if(typeNameConst)
   {
      CflatSetFlag(typeUsage.mFlags, TypeUsageFlags::Const);
      baseTypeNameStart = typeNameConst + 6u;
   }

   // is it a pointer?
   const char* typeNamePtr = strchr(baseTypeNameStart, '*');

   if(typeNamePtr)
   {
      typeUsage.mPointerLevel++;
      baseTypeNameEnd = typeNamePtr - 1u;
   }
   else
   {
      // is it a reference?
      const char* typeNameRef = strchr(baseTypeNameStart, '&');

      if(typeNameRef)
      {
         CflatSetFlag(typeUsage.mFlags, TypeUsageFlags::Reference);
         baseTypeNameEnd = typeNameRef - 1u;
      }
   }

   // remove empty spaces
   while(baseTypeNameStart[0] == ' ')
   {
      baseTypeNameStart++;
   }

   while(baseTypeNameEnd[0] == ' ')
   {
      baseTypeNameEnd--;
   }

   // assign the type
   char baseTypeName[32];
   size_t baseTypeNameLength = baseTypeNameEnd - baseTypeNameStart + 1u;
   strncpy(baseTypeName, baseTypeNameStart, baseTypeNameLength);
   baseTypeName[baseTypeNameLength] = '\0';

   typeUsage.mType = getType(baseTypeName);

   return typeUsage;
}

bool Environment::load(const char* pProgramName, const char* pCode)
{
   const uint32_t programNameHash = hash(pProgramName);
   ProgramsRegistry::iterator it = mPrograms.find(programNameHash);

   if(it == mPrograms.end())
   {
      mPrograms[programNameHash] = Program();
      it = mPrograms.find(programNameHash);
   }

   Program& program = it->second;
   program.~Program();

   strcpy(program.mName, pProgramName);
   program.mCode.assign(pCode);
   program.mCode.shrink_to_fit();

   mErrorMessage.clear();

   ParsingContext parsingContext;

   preprocess(parsingContext, pCode);
   tokenize(parsingContext);
   parse(parsingContext, program);

   if(!parsingContext.mErrorMessage.empty())
   {
      mErrorMessage.assign(parsingContext.mErrorMessage);
      return false;
   }

   mGlobalNamespace.releaseInstances(0u);

   mExecutionContext.mCurrentLine = 0u;
   mExecutionContext.mJumpStatement = JumpStatement::None;

   execute(mExecutionContext, program);

   if(!mExecutionContext.mErrorMessage.empty())
   {
      mErrorMessage.assign(mExecutionContext.mErrorMessage);
      return false;
   }

   return true;
}

const char* Environment::getErrorMessage()
{
   return mErrorMessage.empty() ? nullptr : mErrorMessage.c_str();
}
