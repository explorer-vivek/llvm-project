//===--- ReplaceBindNCheck.cpp - clang-tidy -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ReplaceBindnCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void ReplaceBindnCheck::registerMatchers(MatchFinder *Finder) {
  // Match std::bind1st and std::bind2nd, including template instantiations
  auto BindNMatcher = 
      callExpr(
          callee(
              functionDecl(
                  hasAnyName("::std::bind1st", "::std::bind2nd")
              ).bind("bindFunc")
          ),
          // Match both direct template arguments and deduced types
          hasArgument(0, expr().bind("func")),
          hasArgument(1, expr().bind("value")),
          // Optionally bind the parent expression for context
          optionally(hasAncestor(expr().bind("parent")))
      ).bind("bindCall");

  // Match the expressions, including those nested within other expressions
  Finder->addMatcher(
      traverse(TK_AsIs,
          expr(anyOf(BindNMatcher,
                    forEachDescendant(BindNMatcher)))
      ),
      this);
}

void ReplaceBindnCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *BindCall = Result.Nodes.getNodeAs<CallExpr>("bindCall");
  const auto *BindFunc = Result.Nodes.getNodeAs<FunctionDecl>("bindFunc");
  const auto *Func = Result.Nodes.getNodeAs<Expr>("func");
  const auto *Value = Result.Nodes.getNodeAs<Expr>("value");
  
  if (!BindCall || !BindFunc || !Func || !Value)
    return;

  // Skip if we're in a macro expansion
  if (BindCall->getBeginLoc().isMacroID())
    return;

  StringRef FuncName = BindFunc->getName();
  bool IsBind1st = FuncName == "bind1st";

  // Get source text for the function and value, preserving template arguments
  const SourceManager &SM = *Result.SourceManager;
  const LangOptions &LangOpts = getLangOpts();

  // Get the exact source text including any template arguments
  CharSourceRange FuncRange = CharSourceRange::getTokenRange(
      Func->getBeginLoc(),
      Func->getEndLoc());
  CharSourceRange ValueRange = CharSourceRange::getTokenRange(
      Value->getBeginLoc(),
      Value->getEndLoc());

  StringRef FuncText = Lexer::getSourceText(FuncRange, SM, LangOpts);
  StringRef ValueText = Lexer::getSourceText(ValueRange, SM, LangOpts);

  if (FuncText.empty() || ValueText.empty())
    return;

  // Create replacement text: std::bind(func, value, _1) for bind1st
  // or std::bind(func, _1, value) for bind2nd
  std::string Replacement = "std::bind(" + FuncText.str() + ", ";
  if (IsBind1st) {
    Replacement += ValueText.str() + ", std::placeholders::_1)";
  } else {
    Replacement += "std::placeholders::_1, " + ValueText.str() + ")";
  }

  diag(BindCall->getBeginLoc(), 
       "%0 is deprecated; use std::bind instead")
      << FuncName
      << FixItHint::CreateReplacement(BindCall->getSourceRange(), Replacement);
}

} // namespace clang::tidy::modernize
