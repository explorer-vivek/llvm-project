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
  // Match boost::bind
  auto BoostBindMatcher = 
      callExpr(
          callee(
              functionDecl(
                  hasName("::boost::bind")
              ).bind("bindFunc")
          )
      ).bind("bindCall");

  // Match boost::placeholders::_N using regex
  auto BoostPlaceholderMatcher =
      declRefExpr(
          hasDeclaration(
              namedDecl(
                  matchesName("::boost::placeholders::_[1-9]")
              )
          )
      ).bind("placeholder");

  // Match the expressions, including those nested within other expressions
  Finder->addMatcher(
      traverse(TK_AsIs,
          expr(
              anyOf(
                  BoostBindMatcher,
                  BoostPlaceholderMatcher,
                  forEachDescendant(stmt(anyOf(BoostBindMatcher, BoostPlaceholderMatcher)))
              )
          )
      ),
      this);
}

void ReplaceBoostBindCheck::check(const MatchFinder::MatchResult &Result) {
  // Handle boost::bind replacement
  if (const auto *BindCall = Result.Nodes.getNodeAs<CallExpr>("bindCall")) {
    if (const auto *BindFunc = Result.Nodes.getNodeAs<FunctionDecl>("bindFunc")) {
      SourceRange Range = BindCall->getCallee()->getSourceRange();
      diag(BindCall->getBeginLoc(), "use std::bind instead of boost::bind")
          << FixItHint::CreateReplacement(Range, "std::bind");
    }
  }

  // Handle boost::placeholders replacement
  if (const auto *Placeholder = Result.Nodes.getNodeAs<DeclRefExpr>("placeholder")) {
    StringRef Name = Placeholder->getDecl()->getName();
    std::string Replacement = "std::placeholders::" + Name.str();
    
    SourceRange Range = Placeholder->getSourceRange();
    diag(Placeholder->getBeginLoc(), 
         "use std::placeholders::%0 instead of boost::placeholders::%0")
        << Name
        << FixItHint::CreateReplacement(Range, Replacement);
  }
}

} // namespace clang::tidy::modernize
