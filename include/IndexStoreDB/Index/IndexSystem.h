//===--- IndexSystem.h ------------------------------------------*- C++ -*-===//
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

#ifndef INDEXSTOREDB_INDEX_INDEXSYSTEM_H
#define INDEXSTOREDB_INDEX_INDEXSYSTEM_H

#include "IndexStoreDB/Support/LLVM.h"
#include "IndexStoreDB/Support/Visibility.h"
#include "llvm/ADT/OptionSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Chrono.h"
#include <memory>
#include <string>
#include <vector>

namespace IndexStoreDB {
  class CanonicalFilePathRef;
  class SymbolOccurrence;
  enum class SymbolRole : uint64_t;
  enum class SymbolKind : uint8_t;
  typedef std::shared_ptr<SymbolOccurrence> SymbolOccurrenceRef;
  typedef llvm::OptionSet<SymbolRole> SymbolRoleSet;

namespace index {

  class SymbolDataProvider;
  class IndexSystemDelegate;
  typedef std::shared_ptr<SymbolDataProvider> SymbolDataProviderRef;
  struct StoreUnitInfo;
  class IndexStoreLibraryProvider;

class INDEXSTOREDB_EXPORT IndexSystem {
public:
  ~IndexSystem();

  static std::shared_ptr<IndexSystem> create(StringRef StorePath,
                                             StringRef dbasePath,
                                             std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
                                             std::shared_ptr<IndexSystemDelegate> Delegate,
                                             bool useExplicitOutputUnits,
                                             bool readonly,
                                             bool enableOutOfDateFileWatching,
                                             bool listenToUnitEvents,
                                             Optional<size_t> initialDBSize,
                                             std::string &Error);

  void waitUntilDoneInitializing();

  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime);

  /// Check whether any unit(s) containing \p file are out of date and if so,
  /// *synchronously* notify the delegate.
  void checkUnitContainingFileIsOutOfDate(StringRef file);

  void registerMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);
  void unregisterMainFiles(ArrayRef<StringRef> filePaths, StringRef productName);

  /// Add output filepaths for the set of unit files that index data should be loaded from.
  /// Only has an effect if `useExplicitOutputUnits` was set to true at initialization.
  void addUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing = false);

  /// Remove output filepaths from the set of unit files that index data should be loaded from.
  /// Only has an effect if `useExplicitOutputUnits` was set to true at initialization.
  void removeUnitOutFilePaths(ArrayRef<StringRef> filePaths, bool waitForProcessing = false);

  // FIXME: Accept a list of active main files so that it can remove stale unit
  // files.
  void purgeStaleData();

  /// *For Testing* Poll for any changes to units and wait until they have been registered.
  void pollForUnitChangesAndWait();

  void printStats(raw_ostream &OS);

  void dumpProviderFileAssociations(raw_ostream &OS);
  void dumpProviderFileAssociations();

  //===--------------------------------------------------------------------===//
  // Queries
  //===--------------------------------------------------------------------===//

  bool foreachSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachRelatedSymbolOccurrenceByUSR(StringRef USR, SymbolRoleSet RoleSet,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachCanonicalSymbolOccurrenceContainingPattern(StringRef Pattern,
                                                bool AnchorStart,
                                                bool AnchorEnd,
                                                bool Subsequence,
                                                bool IgnoreCase,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool foreachCanonicalSymbolOccurrenceByName(StringRef name,
                        function_ref<bool(SymbolOccurrenceRef Occur)> receiver);

  bool foreachSymbolName(function_ref<bool(StringRef name)> receiver);

  bool foreachCanonicalSymbolOccurrenceByUSR(StringRef USR,
                        function_ref<bool(SymbolOccurrenceRef occur)> receiver);

  bool foreachSymbolCallOccurrence(SymbolOccurrenceRef Callee,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  size_t countOfCanonicalSymbolsWithKind(SymbolKind symKind, bool workspaceOnly);
  bool foreachCanonicalSymbolOccurrenceByKind(SymbolKind symKind, bool workspaceOnly,
                        function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

  bool isKnownFile(StringRef filePath);

  bool foreachMainUnitContainingFile(StringRef filePath,
                             function_ref<bool(const StoreUnitInfo &unitInfo)> receiver);

  bool foreachFileOfUnit(StringRef unitName,
                         bool followDependencies,
                         function_ref<bool(CanonicalFilePathRef filePath)> receiver);

  bool foreachFilenameContainingPattern(StringRef Pattern,
                                        bool AnchorStart,
                                        bool AnchorEnd,
                                        bool Subsequence,
                                        bool IgnoreCase,
                               function_ref<bool(CanonicalFilePathRef FilePath)> Receiver);

  bool foreachFileIncludingFile(StringRef TargetPath,
                                             function_ref<bool(CanonicalFilePathRef SourcePath, unsigned Line)> Receiver);

  bool foreachFileIncludedByFile(StringRef SourcePath,
                                              function_ref<bool(CanonicalFilePathRef TargetPath, unsigned Line)> Receiver);

  /// Returns unit test class/method occurrences that are referenced from units associated with the provided output file paths.
  /// \returns `false` if the receiver returned `false` to stop receiving symbols, `true` otherwise.
  bool foreachUnitTestSymbolReferencedByOutputPaths(ArrayRef<CanonicalFilePathRef> FilePaths,
      function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

private:
  IndexSystem(void *Impl) : Impl(Impl) {}

  void *Impl; // An IndexSystemImpl.
};

} // namespace index
} // namespace IndexStoreDB

#endif
