// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FRONTENDACTIONFACTORY_H
#define CPPSCANNER_FRONTENDACTIONFACTORY_H

#include "indexer.h"

#include <clang/Tooling/Tooling.h>
#include <clang/Index/IndexingAction.h>

namespace cppscanner
{

class IndexingFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
private:
  std::shared_ptr<Indexer> m_IndexDataConsumer;
  bool m_indexLocalSymbols = false;

public:
  IndexingFrontendActionFactory(std::shared_ptr<Indexer> dataConsumer)
  {
    m_IndexDataConsumer = dataConsumer;
  }

  void setIndexLocalSymbols(bool on)
  {
    m_indexLocalSymbols = on;
  }

  std::unique_ptr<clang::FrontendAction> create() final
  {
    auto idc = m_IndexDataConsumer;

    clang::index::IndexingOptions opts; // TODO: review which options we should enable
    opts.IndexFunctionLocals = m_indexLocalSymbols;
    opts.IndexParametersInDeclarations = false;
    opts.IndexTemplateParameters = m_indexLocalSymbols;

    opts.ShouldTraverseDecl = [idc](const clang::Decl* decl) -> bool {
      return idc->ShouldTraverseDecl(decl);
      };

    return clang::index::createIndexingAction(idc, opts);
  }
};

} // namespace cppscanner

#endif // CPPSCANNER_FRONTENDACTIONFACTORY_H
