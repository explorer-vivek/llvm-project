//===--- ReplaceBoostRefCheck.cpp - clang-tidy ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ReplaceBoostRefCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void ReplaceBoostRefCheck::registerMatchers(MatchFinder *Finder) {
  // Match only boost::ref and boost::cref
  auto BoostRefMatcher = 
      callExpr(
          callee(
              functionDecl(
                  hasAnyName("::boost::ref", "::boost::cref")
              ).bind("refFunc")
          )
      ).bind("refCall");

  // Match the function calls, including those nested within other expressions
  Finder->addMatcher(
      traverse(TK_AsIs,
          expr(
              anyOf(
                  BoostRefMatcher,
                  expr(forEachDescendant(BoostRefMatcher))
              )
          )
      ),
      this);
}

void ReplaceBoostRefCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *RefCall = Result.Nodes.getNodeAs<CallExpr>("refCall");
  const auto *RefFunc = Result.Nodes.getNodeAs<FunctionDecl>("refFunc");
  
  if (!RefCall || !RefFunc)
    return;

  StringRef Name = RefFunc->getName();
  std::string Replacement = "std::" + Name.str();

  // Get the source range for just the function name and qualifier
  SourceRange Range = RefCall->getCallee()->getSourceRange();

  diag(RefCall->getBeginLoc(), "use %0 instead of boost::%1")
      << Replacement << Name
      << FixItHint::CreateReplacement(Range, Replacement);
}

} // namespace clang::tidy::modernize
