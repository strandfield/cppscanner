// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_ASTVISITOR_H
#define CPPSCANNER_ASTVISITOR_H

#include <clang/AST/RecursiveASTVisitor.h>

namespace clang
{

} // namespace clang

namespace cppscanner
{

class Indexer;

class ClangAstVisitor : public clang::RecursiveASTVisitor<ClangAstVisitor>
{
private:
  Indexer& m_indexer;

public:
  explicit ClangAstVisitor(Indexer& idxr);

  bool VisitCallExpr(clang::CallExpr* e);
};

} // namespace cppscanner

#endif // CPPSCANNER_ASTVISITOR_H
