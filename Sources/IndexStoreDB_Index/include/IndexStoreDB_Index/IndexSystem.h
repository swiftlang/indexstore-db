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

#include <IndexStoreDB_Support/LLVM.h>
#include <IndexStoreDB_Support/Visibility.h>
#include <IndexStoreDB_Index/IndexStoreCXX.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_OptionSet.h>
#include <IndexStoreDB_LLVMSupport/llvm_ADT_StringRef.h>
#include <IndexStoreDB_LLVMSupport/llvm_Support_Chrono.h>
#include <memory>
#include <string>
#include <vector>

namespace IndexStoreDB {
  class CanonicalFilePathRef;
  class SymbolOccurrence;
  class Symbol;

  enum class SymbolRole : uint64_t;
  enum class SymbolKind : uint8_t;

  typedef std::shared_ptr<SymbolOccurrence> SymbolOccurrenceRef;
  typedef std::shared_ptr<Symbol> SymbolRef;

  typedef llvm::OptionSet<SymbolRole> SymbolRoleSet;

namespace index {

  class SymbolDataProvider;
  class IndexSystemDelegate;
  typedef std::shared_ptr<SymbolDataProvider> SymbolDataProviderRef;
  struct StoreUnitInfo;
  class IndexStoreLibraryProvider;

struct CreationOptions {
  indexstore::IndexStoreCreationOptions indexStoreOptions;
  bool useExplicitOutputUnits = false;
  bool wait = false;
  bool readonly = false;
  bool enableOutOfDateFileWatching = false;
  bool listenToUnitEvents = true;
};

class INDEXSTOREDB_EXPORT IndexSystem {
public:
  ~IndexSystem();

  static std::shared_ptr<IndexSystem> create(StringRef StorePath,
                                             StringRef dbasePath,
                                             std::shared_ptr<IndexStoreLibraryProvider> storeLibProvider,
                                             std::shared_ptr<IndexSystemDelegate> Delegate,
                                             const CreationOptions &options,
                                             Optional<size_t> initialDBSize,
                                             std::string &Error);

  bool isUnitOutOfDate(StringRef unitOutputPath, ArrayRef<StringRef> dirtyFiles);
  bool isUnitOutOfDate(StringRef unitOutputPath, llvm::sys::TimePoint<> outOfDateModTime);
  llvm::Optional<llvm::sys::TimePoint<>> timestampOfUnitForOutputPath(StringRef unitOutputPath);

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
  void pollForUnitChangesAndWait(bool isInitialScan);

  void printStats(raw_ostream &OS);

  void dumpProviderFileAssociations(raw_ostream &OS);
  void dumpProviderFileAssociations();

  void addDelegate(std::shared_ptr<IndexSystemDelegate> Delegate);

  //===--------------------------------------------------------------------===//
  // Queries
  //===--------------------------------------------------------------------===//

  bool foreachSymbolInFilePath(StringRef FilePath,
                               function_ref<bool(SymbolRef Symbol)> Receiver);

  bool foreachSymbolOccurrenceInFilePath(StringRef FilePath,
                                         function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);

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

  bool foreachIncludeOfUnit(StringRef unitName,
                            function_ref<bool(CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)> receiver);

  /// Returns unit test class/method occurrences that are referenced from units associated with the provided output file paths.
  /// \returns `false` if the receiver returned `false` to stop receiving symbols, `true` otherwise.
  bool foreachUnitTestSymbolReferencedByOutputPaths(ArrayRef<CanonicalFilePathRef> FilePaths,
      function_ref<bool(SymbolOccurrenceRef Occur)> Receiver);


  /// Calls `receiver` for every unit test symbol in unit files that reference
  /// one of the main files in `mainFilePaths`.
  ///
  ///  \returns `false` if the receiver returned `false` to stop receiving symbols, `true` otherwise.
  bool foreachUnitTestSymbolReferencedByMainFiles(
      ArrayRef<StringRef> mainFilePaths,
      function_ref<bool(SymbolOccurrenceRef Occur)> receiver
  );

  /// Calls `receiver` for every unit test symbol in the index.
  ///
  ///  \returns `false` if the receiver returned `false` to stop receiving symbols, `true` otherwise.
  bool foreachUnitTestSymbol(function_ref<bool(SymbolOccurrenceRef Occur)> receiver);

  /// Returns the latest modification date of a unit that contains the given source file.
  ///
  /// If no unit containing the given source file exists, returns `None`.
  llvm::Optional<llvm::sys::TimePoint<>> timestampOfLatestUnitForFile(StringRef filePath);
private:
  IndexSystem(void *Impl) : Impl(Impl) {}

  void *Impl; // An IndexSystemImpl.
};

} // namespace index
} // namespace IndexStoreDB

#endif
