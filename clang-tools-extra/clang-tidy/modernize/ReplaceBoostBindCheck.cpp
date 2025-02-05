//===--- ReplaceBoostBindCheck.cpp - clang-tidy ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ReplaceBoostBindCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void ReplaceBoostBindCheck::registerMatchers(MatchFinder *Finder) {
  // Match only boost::bind
  auto BoostBindMatcher = 
      callExpr(
          callee(
              functionDecl(
                  hasName("::boost::bind")
              ).bind("bindFunc")
          )
      ).bind("bindCall");

  // Match the function calls, including those nested within other expressions
  Finder->addMatcher(
      traverse(TK_AsIs,
          expr(
              anyOf(
                  BoostBindMatcher,
                  expr(forEachDescendant(BoostBindMatcher))
              )
          )
      ),
      this);
}

void ReplaceBoostBindCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *BindCall = Result.Nodes.getNodeAs<CallExpr>("bindCall");
  const auto *BindFunc = Result.Nodes.getNodeAs<FunctionDecl>("bindFunc");
  
  if (!BindCall || !BindFunc)
    return;

  // Get the source range for just the function name and qualifier
  SourceRange Range = BindCall->getCallee()->getSourceRange();

  diag(BindCall->getBeginLoc(), "use std::bind instead of boost::bind")
      << FixItHint::CreateReplacement(Range, "std::bind");
}

} // namespace clang::tidy::modernize
