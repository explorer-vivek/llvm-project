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
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "../utils/UsingInserter.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

class ReplaceBoostBindCheck::Implementation {
public:
  Implementation(const SourceManager &SM) : SourceMgr(SM) {}
  
  bool hasUsingDirective(const CompoundStmt *Scope) const {
    return InsertedScopes.count(Scope) > 0;
  }
  
  void markUsingDirectiveInserted(const CompoundStmt *Scope) {
    InsertedScopes.insert(Scope);
  }

private:
  const SourceManager &SourceMgr;
  llvm::DenseSet<const CompoundStmt *> InsertedScopes;
};

ReplaceBoostBindCheck::ReplaceBoostBindCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context), Impl(nullptr) {}

ReplaceBoostBindCheck::~ReplaceBoostBindCheck() = default;

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

  // Match boost::placeholders::_N using regex, without requiring compound statement
  auto BoostPlaceholderMatcher =
      declRefExpr(
          hasDeclaration(
              namedDecl(
                  matchesName("::boost::placeholders::_[1-9]")
              )
          ),
          // Optionally match the innermost compound statement if it exists
          optionally(
              hasAncestor(compoundStmt().bind("scope"))
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
  // Initialize the Implementation on first use
  if (!Impl)
    Impl = std::make_unique<Implementation>(*Result.SourceManager);

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
    SourceRange Range = Placeholder->getSourceRange();

    const auto *Scope = Result.Nodes.getNodeAs<CompoundStmt>("scope");
    if (!Scope) {
      // No parent scope found - use fully qualified name
      diag(Placeholder->getBeginLoc(), 
           "warning: boost::placeholders::%0 used outside of a compound statement, "
           "using fully qualified std::placeholders::%0")
          << Name;
      std::string Replacement = "std::placeholders::" + Name.str();
      diag(Placeholder->getBeginLoc(), 
           "use std::placeholders::%0 instead of boost::placeholders::%0")
          << Name
          << FixItHint::CreateReplacement(Range, Replacement);
      return;
    }

    if (!Impl->hasUsingDirective(Scope)) {
      // Add using directive for top-level scope
      const SourceManager &SM = *Result.SourceManager;
      SourceLocation InsertLoc = Scope->getLBracLoc().getLocWithOffset(1);
      
      // Find the first non-brace token in this scope to copy its indentation
      SourceLocation FirstTokenLoc;
      for (const auto *Child : Scope->children()) {
        if (Child) {
          FirstTokenLoc = Child->getBeginLoc();
          break;
        }
      }

      std::string Indent;
      if (FirstTokenLoc.isValid()) {
        bool Invalid = false;
        const char *TokenPtr = SM.getCharacterData(FirstTokenLoc, &Invalid);
        if (!Invalid) {
          const char *LineStart = TokenPtr;
          while (LineStart > SM.getCharacterData(Scope->getLBracLoc()) && 
                 (LineStart[-1] == ' ' || LineStart[-1] == '\t')) {
            --LineStart;
          }
          Indent = std::string(LineStart, TokenPtr);
        }
      }
      
      diag(InsertLoc, "add using directive for std::placeholders")
          << FixItHint::CreateInsertion(
                 InsertLoc,
                 "\n" + Indent + "using namespace std::placeholders;\n");
      
      Impl->markUsingDirectiveInserted(Scope);

      // Use unqualified name when we have the using directive
      diag(Placeholder->getBeginLoc(), 
           "use %0 instead of boost::placeholders::%0")
          << Name
          << FixItHint::CreateReplacement(Range, Name);
    }
  }
}

} // namespace clang::tidy::modernize
