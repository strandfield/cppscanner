// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "astvisitor.h"

#include "indexer.h"

namespace cppscanner
{

ClangAstVisitor::ClangAstVisitor(Indexer& idxr) :
  m_indexer(idxr)
{

}

bool ClangAstVisitor::VisitCallExpr(clang::CallExpr* e)
{
  clang::FileID file = m_indexer.getAstContext()->getSourceManager().getFileID(e->getBeginLoc());

  if (!m_indexer.shouldIndexFile(file))
  {
    return true;
  }

  // The following is heavily inspired by woboq's BrowserASTVisitor::VisitCallExpr().
  // It is used to list places where arguments are passed to a function by non-const reference.
  auto decl = e->getDirectCallee();
  if (decl && !llvm::isa<clang::CXXOperatorCallExpr>(e)) {
    for (unsigned int i = 0; decl && i < e->getNumArgs() && i < decl->getNumParams(); ++i) {
      const clang::ParmVarDecl* paramDecl = decl->getParamDecl(i);
      const clang::QualType& type = paramDecl->getType();
      if (type->isLValueReferenceType() && !type.getNonReferenceType().isConstQualified()) {
        const clang::SourceLocation& loc = e->getArg(i)->getSourceRange().getBegin();
        ArgumentPassedByReference refarg;
        std::tie(refarg.fileID, refarg.position) = m_indexer.convert(file, loc);
        m_indexer.getCurrentIndex()->add(refarg);
      }
    }
  }

  return true;
}

} // namespace cppscanner
