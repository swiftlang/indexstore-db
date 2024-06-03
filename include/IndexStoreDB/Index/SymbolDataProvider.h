//===--- SymbolDataProvider.h -----------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef INDEXSTOREDB_INDEX_SYMBOLDATAPROVIDER_H
#define INDEXSTOREDB_INDEX_SYMBOLDATAPROVIDER_H

#include "IndexStoreDB/Support/LLVM.h"
#include "llvm/ADT/OptionSet.h"
#include <memory>
#include <vector>

namespace IndexStoreDB {
  class Symbol;
  class SymbolOccurrence;
  struct SymbolInfo;
  enum class SymbolProviderKind : uint8_t;
  enum class SymbolRole : uint64_t;
  typedef std::shared_ptr<Symbol> SymbolRef;
  typedef std::shared_ptr<SymbolOccurrence> SymbolOccurrenceRef;
  typedef llvm::OptionSet<SymbolRole> SymbolRoleSet;

namespace db {
  class IDCode;
}

namespace index {

class SymbolDataProvider {
public:
  virtual ~SymbolDataProvider() {}

  virtual std::string getIdentifier() const = 0;

  virtual bool isSystem() const = 0;

  virtual bool foreachCoreSymbolData(function_ref<bool(StringRef USR,
                                                       StringRef Name,
                                                       SymbolInfo Info,
                                                       SymbolRoleSet Roles,
                                                       SymbolRoleSet RelatedRoles)> Receiver) = 0;

  virtual bool foreachSymbolOccurrence(function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) = 0;

  virtual bool foreachSymbolOccurrenceByUSR(ArrayRef<db::IDCode> USRs,
                                            SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) = 0;

  virtual bool foreachRelatedSymbolOccurrenceByUSR(ArrayRef<db::IDCode> USRs,
                                            SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) = 0;

  virtual bool foreachUnitTestSymbolOccurrence(
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver) = 0;

private:
  virtual void anchor();
};

typedef std::shared_ptr<SymbolDataProvider> SymbolDataProviderRef;

} // namespace index
} // namespace IndexStoreDB

#endif
