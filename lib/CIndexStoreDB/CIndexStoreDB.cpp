//===--- CIndexStoreDB.cpp ------------------------------------------------===//
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

#include "CIndexStoreDB/CIndexStoreDB.h"
#include "CIndexStoreDB/CIndexStoreDB_Internal.h"
#include "IndexStoreDB/Index/IndexStoreLibraryProvider.h"
#include "IndexStoreDB/Index/IndexSystem.h"
#include "IndexStoreDB/Index/IndexSystemDelegate.h"
#include "IndexStoreDB/Support/Path.h"
#include "IndexStoreDB/Core/Symbol.h"
#include "indexstore/IndexStoreCXX.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <Block.h>

using namespace IndexStoreDB;
using namespace index;
using namespace IndexStoreDB::internal;

static indexstoredb_symbol_kind_t toCSymbolKind(SymbolKind K);

namespace {

struct IndexStoreDBError {
  std::string message;
  IndexStoreDBError(StringRef message) : message(message.str()) {}
};

class BlockIndexStoreLibraryProvider : public IndexStoreLibraryProvider {
  indexstore_library_provider_t callback;

public:
  BlockIndexStoreLibraryProvider(indexstore_library_provider_t callback)
      : callback(Block_copy(callback)) {}
  ~BlockIndexStoreLibraryProvider() {
    Block_release(callback);
  }

  IndexStoreLibraryRef getLibraryForStorePath(StringRef storePath) override {
    indexstore_functions_t api;
    if (auto lib = callback(storePath.str().c_str())) {
      auto *obj = (Object<IndexStoreLibraryRef> *)lib;
      return obj->value;
    } else {
      return nullptr;
    }
  }
};

struct DelegateEvent {
  indexstoredb_delegate_event_kind_t kind;
  uint64_t count;
  StoreUnitInfo *outOfDateUnitInfo = nullptr;
  uint64_t outOfDateModTime = 0;
  std::string outOfDateTriggerFile;
  std::string outOfDateTriggerDescription;
  bool outOfDateIsSynchronous = false;
};

class BlockIndexSystemDelegate: public IndexSystemDelegate {
  indexstoredb_delegate_event_receiver_t callback;
public:
  BlockIndexSystemDelegate(indexstoredb_delegate_event_receiver_t callback) : callback(Block_copy(callback)) {}
  ~BlockIndexSystemDelegate() { Block_release(callback); }

  void processingAddedPending(unsigned NumActions) override {
    DelegateEvent event{INDEXSTOREDB_EVENT_PROCESSING_ADDED_PENDING, NumActions};
    callback(&event);
  }
  void processingCompleted(unsigned NumActions) override {
    DelegateEvent event{INDEXSTOREDB_EVENT_PROCESSING_COMPLETED, NumActions};
    callback(&event);
  }

  void unitIsOutOfDate(StoreUnitInfo unitInfo,
                       llvm::sys::TimePoint<> outOfDateModTime,
                       OutOfDateTriggerHintRef hint,
                       bool synchronous) override {
    DelegateEvent event{INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE, 0,
      &unitInfo,
      (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(outOfDateModTime.time_since_epoch()).count(),
      hint->originalFileTrigger(),
      hint->description(),
      synchronous
    };
    callback(&event);
  }
};

} // end anonymous namespace

indexstoredb_creation_options_t
indexstoredb_creation_options_create(void) {
  return new CreationOptions();
}

void
indexstoredb_creation_options_dispose(indexstoredb_creation_options_t c_options) {
  auto *options = static_cast<CreationOptions *>(c_options);
  delete options;
}

void
indexstoredb_creation_options_add_prefix_mapping(indexstoredb_creation_options_t c_options,
                                                 const char *path_prefix,
                                                 const char *remapped_path_prefix) {
  auto *options = static_cast<CreationOptions *>(c_options);
  options->indexStoreOptions.addPrefixMapping(path_prefix, remapped_path_prefix);
}

void
indexstoredb_creation_options_listen_to_unit_events(indexstoredb_creation_options_t c_options,
                                                        bool listenToUnitEvents) {
  auto *options = static_cast<CreationOptions *>(c_options);
  options->listenToUnitEvents = listenToUnitEvents;
}

void
indexstoredb_creation_options_enable_out_of_date_file_watching(indexstoredb_creation_options_t c_options,
                                                               bool enableOutOfDateFileWatching) {
  auto *options = static_cast<CreationOptions *>(c_options);
  options->enableOutOfDateFileWatching = enableOutOfDateFileWatching;
}

void
indexstoredb_creation_options_readonly(indexstoredb_creation_options_t c_options,
                                           bool readonly) {
  auto *options = static_cast<CreationOptions *>(c_options);
  options->readonly = readonly;
}

void
indexstoredb_creation_options_wait(indexstoredb_creation_options_t c_options,
                                       bool wait) {
  auto *options = static_cast<CreationOptions *>(c_options);
  options->wait = wait;
}

void
indexstoredb_creation_options_use_explicit_output_units(indexstoredb_creation_options_t c_options,
                                                            bool useExplicitOutputUnits) {
  auto *options = static_cast<CreationOptions *>(c_options);
  options->useExplicitOutputUnits = useExplicitOutputUnits;
}

indexstoredb_index_t
indexstoredb_index_create(const char *storePath, const char *databasePath,
                          indexstore_library_provider_t libProvider,
                          indexstoredb_delegate_event_receiver_t delegateCallback,
                          indexstoredb_creation_options_t cOptions,
                          indexstoredb_error_t *error) {

  auto delegate = std::make_shared<BlockIndexSystemDelegate>(delegateCallback);
  auto libProviderObj = std::make_shared<BlockIndexStoreLibraryProvider>(libProvider);
  auto options = static_cast<CreationOptions *>(cOptions);

  std::string errMsg;
  if (auto index =
          IndexSystem::create(storePath, databasePath, libProviderObj, delegate,
                              *options, llvm::None, errMsg)) {

    return make_object(index);

  } else if (error) {
    *error = (indexstoredb_error_t)new IndexStoreDBError(errMsg);
  }
  return nullptr;
}

void indexstoredb_index_add_delegate(indexstoredb_index_t index,
                                     indexstoredb_delegate_event_receiver_t delegateCallback) {
  auto delegate = std::make_shared<BlockIndexSystemDelegate>(delegateCallback);
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  obj->value->addDelegate(std::move(delegate));
}

indexstoredb_indexstore_library_t
indexstoredb_load_indexstore_library(const char *dylibPath,
                                     indexstoredb_error_t *error) {
  std::string errMsg;
  if (auto lib = loadIndexStoreLibrary(dylibPath, errMsg)) {
    return make_object(lib);
  } else if (error) {
    *error = (indexstoredb_error_t)new IndexStoreDBError(errMsg);
  }
  return nullptr;
}

unsigned indexstoredb_format_version(indexstoredb_indexstore_library_t lib) {
  auto obj = (Object<std::shared_ptr<indexstore::IndexStoreLibrary>> *)lib;
  return obj->value->api().format_version();
}

unsigned indexstoredb_store_version(indexstoredb_indexstore_library_t lib) {
  auto obj = (Object<std::shared_ptr<indexstore::IndexStoreLibrary>> *)lib;
  const indexstore_functions_t &api = obj->value->api();
  if (api.version) {
    return api.version();
  }
  return 0;
}

void indexstoredb_index_poll_for_unit_changes_and_wait(indexstoredb_index_t index, bool isInitialScan) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  obj->value->pollForUnitChangesAndWait(isInitialScan);
}

void indexstoredb_index_add_unit_out_file_paths(indexstoredb_index_t index,
                                                const char *const *paths, size_t count,
                                                bool waitForProcessing) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  SmallVector<StringRef, 32> strVec;
  strVec.reserve(count);
  for (unsigned i = 0; i != count; ++i)
    strVec.push_back(paths[i]);
  return obj->value->addUnitOutFilePaths(strVec, waitForProcessing);
}

void indexstoredb_index_remove_unit_out_file_paths(indexstoredb_index_t index,
                                                   const char *const *paths, size_t count,
                                                   bool waitForProcessing) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  SmallVector<StringRef, 32> strVec;
  strVec.reserve(count);
  for (unsigned i = 0; i != count; ++i)
    strVec.push_back(paths[i]);
  return obj->value->removeUnitOutFilePaths(strVec, waitForProcessing);
}

indexstoredb_delegate_event_kind_t
indexstoredb_delegate_event_get_kind(indexstoredb_delegate_event_t event) {
  return reinterpret_cast<DelegateEvent *>(event)->kind;
}

uint64_t indexstoredb_delegate_event_get_count(indexstoredb_delegate_event_t event) {
  return reinterpret_cast<DelegateEvent *>(event)->count;
}

indexstoredb_unit_info_t
indexstoredb_delegate_event_get_outofdate_unit_info(indexstoredb_delegate_event_t event) {
  return reinterpret_cast<DelegateEvent *>(event)->outOfDateUnitInfo;
}

uint64_t indexstoredb_delegate_event_get_outofdate_modtime(indexstoredb_delegate_event_t event) {
  return reinterpret_cast<DelegateEvent *>(event)->outOfDateModTime;
}

bool indexstoredb_delegate_event_get_outofdate_is_synchronous(indexstoredb_delegate_event_t event) {
  return reinterpret_cast<DelegateEvent *>(event)->outOfDateIsSynchronous;
}

const char *
indexstoredb_delegate_event_get_outofdate_trigger_original_file(indexstoredb_delegate_event_t event) {
  DelegateEvent *evt = reinterpret_cast<DelegateEvent *>(event);
  if (evt->outOfDateUnitInfo)
    return evt->outOfDateTriggerFile.c_str();
  else
    return nullptr;
}

const char *
indexstoredb_delegate_event_get_outofdate_trigger_description(indexstoredb_delegate_event_t event) {
  DelegateEvent *evt = reinterpret_cast<DelegateEvent *>(event);
  if (evt->outOfDateUnitInfo)
    return evt->outOfDateTriggerDescription.c_str();
  else
    return nullptr;
}

bool
indexstoredb_index_symbol_occurrences_by_usr(
    indexstoredb_index_t index,
    const char *usr,
    uint64_t roles,
    indexstoredb_symbol_occurrence_receiver_t receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachSymbolOccurrenceByUSR(usr, (SymbolRoleSet)roles,
    [&](SymbolOccurrenceRef Occur) -> bool {
      return receiver((indexstoredb_symbol_occurrence_t)Occur.get());
    });
}

bool
indexstoredb_index_related_symbol_occurrences_by_usr(
    indexstoredb_index_t index,
    const char *usr,
    uint64_t roles,
    indexstoredb_symbol_occurrence_receiver_t receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachRelatedSymbolOccurrenceByUSR(usr, (SymbolRoleSet)roles,
    [&](SymbolOccurrenceRef Occur) -> bool {
      return receiver((indexstoredb_symbol_occurrence_t)Occur.get());
    });
}

bool
indexstoredb_index_symbols_contained_in_file_path(_Nonnull indexstoredb_index_t index,
                                                   const char *_Nonnull path,
                                                   _Nonnull indexstoredb_symbol_receiver_t receiver) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachSymbolInFilePath(path, [&](SymbolRef Symbol) -> bool {
    return receiver((indexstoredb_symbol_receiver_t)Symbol.get());
  });
}

const char *
indexstoredb_symbol_usr(indexstoredb_symbol_t symbol) {
  auto value = (Symbol *)symbol;
  return value->getUSR().c_str();
}

indexstoredb_language_t
indexstoredb_symbol_language(_Nonnull indexstoredb_symbol_t symbol) {
  auto value = (Symbol *)symbol;
  switch (value->getLanguage()) {
  case IndexStoreDB::SymbolLanguage::C:
    return INDEXSTOREDB_LANGUAGE_C;
  case IndexStoreDB::SymbolLanguage::ObjC:
    return INDEXSTOREDB_LANGUAGE_OBJC;
  case IndexStoreDB::SymbolLanguage::CXX:
    return INDEXSTOREDB_LANGUAGE_CXX;
  case IndexStoreDB::SymbolLanguage::Swift:
    return INDEXSTOREDB_LANGUAGE_SWIFT;
  }
}

const char *
indexstoredb_symbol_name(indexstoredb_symbol_t symbol) {
  auto value = (Symbol *)symbol;
  return value->getName().c_str();
}

indexstoredb_symbol_kind_t
indexstoredb_symbol_kind(indexstoredb_symbol_t symbol) {
  auto value = (Symbol *)symbol;
  return toCSymbolKind(value->getSymbolKind());
}

uint64_t
indexstoredb_symbol_properties(indexstoredb_symbol_t symbol) {
  auto value = (Symbol *)symbol;
  return value->getSymbolProperties().toRaw();
}

bool
indexstoredb_index_symbol_names(indexstoredb_index_t index, indexstoredb_symbol_name_receiver receiver) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachSymbolName([&](StringRef ref) -> bool {
    return receiver(ref.str().c_str());
  });
}

bool
indexstoredb_index_canonical_symbol_occurences_by_name(
  indexstoredb_index_t index,
  const char *_Nonnull symbolName,
  indexstoredb_symbol_occurrence_receiver_t receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachCanonicalSymbolOccurrenceByName(symbolName, [&](SymbolOccurrenceRef occur) -> bool {
    return receiver((indexstoredb_symbol_occurrence_t)occur.get());
  });
}

bool
indexstoredb_index_canonical_symbol_occurences_containing_pattern(
  indexstoredb_index_t index,
  const char *_Nonnull pattern,
  bool anchorStart,
  bool anchorEnd,
  bool subsequence,
  bool ignoreCase,
  indexstoredb_symbol_occurrence_receiver_t receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachCanonicalSymbolOccurrenceContainingPattern(
    pattern,
    anchorStart,
    anchorEnd,
    subsequence,
    ignoreCase,
    [&](SymbolOccurrenceRef occur
  ) -> bool {
      return receiver((indexstoredb_symbol_occurrence_t)occur.get());
  });
}

indexstoredb_symbol_t
indexstoredb_symbol_occurrence_symbol(indexstoredb_symbol_occurrence_t occur) {
  auto value = (SymbolOccurrence *)occur;
  return (indexstoredb_symbol_t)value->getSymbol().get();
}

uint64_t
indexstoredb_symbol_relation_get_roles(indexstoredb_symbol_relation_t relation) {
  auto value = (SymbolRelation *)relation;
  return value->getRoles().toRaw();
}

indexstoredb_symbol_t
indexstoredb_symbol_relation_get_symbol(indexstoredb_symbol_relation_t relation) {
  auto value = (SymbolRelation *)relation;
  return (indexstoredb_symbol_t)(value->getSymbol().get());
}

bool
indexstoredb_symbol_occurrence_relations(indexstoredb_symbol_occurrence_t occurrence,
                                         bool(^applier)(indexstoredb_symbol_relation_t)) {
  auto value = (SymbolOccurrence *)occurrence;
  ArrayRef<SymbolRelation> relations = value->getRelations();
  for (auto &rel : relations) {
    if(!applier((indexstoredb_symbol_relation_t)&rel)) {
      return false;
    }
  }
  return true;
}

uint64_t
indexstoredb_symbol_occurrence_roles(indexstoredb_symbol_occurrence_t occur) {
  auto value = (SymbolOccurrence *)occur;
  return (uint64_t)value->getRoles();
}

indexstoredb_symbol_location_t indexstoredb_symbol_occurrence_location(
    indexstoredb_symbol_occurrence_t occur) {
  auto value = (SymbolOccurrence *)occur;
  return (indexstoredb_symbol_location_t)&value->getLocation();
}

const char *
indexstoredb_symbol_location_path(indexstoredb_symbol_location_t loc) {
  auto obj = (SymbolLocation *)loc;
  return obj->getPath().getPathString().c_str();
}

double
indexstoredb_symbol_location_timestamp(indexstoredb_symbol_location_t loc) {
  auto obj = (SymbolLocation *)loc;
  // Up until C++20 the reference date of time_since_epoch is undefined but according to 
  // https://en.cppreference.com/w/cpp/chrono/system_clock most implementations use Unix Time.
  // Since C++20, system_clock is defined to measure time since 1/1/1970.
  // We rely on `time_since_epoch` always returning the nanoseconds since 1/1/1970.
  auto nanosecondsSinceEpoch = obj->getPath().getModificationTime().time_since_epoch().count();
  return static_cast<double>(nanosecondsSinceEpoch) / 1000 / 1000 / 1000;
}

const char *
indexstoredb_symbol_location_module_name(indexstoredb_symbol_location_t loc) {
  auto obj = (SymbolLocation *)loc;
  return obj->getPath().getModuleName().c_str();
}

bool
indexstoredb_symbol_location_is_system(indexstoredb_symbol_location_t loc) {
  auto obj = (SymbolLocation *)loc;
  return obj->isSystem();
}

int
indexstoredb_symbol_location_line(indexstoredb_symbol_location_t loc) {
  auto obj = (SymbolLocation *)loc;
  return obj->getLine();
}

int
indexstoredb_symbol_location_column_utf8(indexstoredb_symbol_location_t loc) {
  auto obj = (SymbolLocation *)loc;
  return obj->getColumn();
}

indexstoredb_object_t indexstoredb_retain(indexstoredb_object_t obj) {
  if (obj)
    ((ObjectBase *)obj)->Retain();
  return obj;
}

void
indexstoredb_release(indexstoredb_object_t obj) {
  if (obj)
    ((ObjectBase *)obj)->Release();
}

const char *
indexstoredb_error_get_description(indexstoredb_error_t error) {
  return ((IndexStoreDBError *)error)->message.c_str();
}

void
indexstoredb_error_dispose(indexstoredb_error_t error) {
  if (error)
   delete (IndexStoreDBError *)error;
}

static indexstoredb_symbol_kind_t toCSymbolKind(SymbolKind K) {
  switch (K) {
  case SymbolKind::Unknown:
    return INDEXSTOREDB_SYMBOL_KIND_UNKNOWN;
  case SymbolKind::Module:
    return INDEXSTOREDB_SYMBOL_KIND_MODULE;
  case SymbolKind::Namespace:
    return INDEXSTOREDB_SYMBOL_KIND_NAMESPACE;
  case SymbolKind::NamespaceAlias:
    return INDEXSTOREDB_SYMBOL_KIND_NAMESPACEALIAS;
  case SymbolKind::Macro:
    return INDEXSTOREDB_SYMBOL_KIND_MACRO;
  case SymbolKind::Enum:
    return INDEXSTOREDB_SYMBOL_KIND_ENUM;
  case SymbolKind::Struct:
    return INDEXSTOREDB_SYMBOL_KIND_STRUCT;
  case SymbolKind::Class:
    return INDEXSTOREDB_SYMBOL_KIND_CLASS;
  case SymbolKind::Protocol:
    return INDEXSTOREDB_SYMBOL_KIND_PROTOCOL;
  case SymbolKind::Extension:
    return INDEXSTOREDB_SYMBOL_KIND_EXTENSION;
  case SymbolKind::Union:
    return INDEXSTOREDB_SYMBOL_KIND_UNION;
  case SymbolKind::TypeAlias:
    return INDEXSTOREDB_SYMBOL_KIND_TYPEALIAS;
  case SymbolKind::Function:
    return INDEXSTOREDB_SYMBOL_KIND_FUNCTION;
  case SymbolKind::Variable:
    return INDEXSTOREDB_SYMBOL_KIND_VARIABLE;
  case SymbolKind::Parameter:
    return INDEXSTOREDB_SYMBOL_KIND_PARAMETER;
  case SymbolKind::Field:
    return INDEXSTOREDB_SYMBOL_KIND_FIELD;
  case SymbolKind::EnumConstant:
    return INDEXSTOREDB_SYMBOL_KIND_ENUMCONSTANT;
  case SymbolKind::InstanceMethod:
    return INDEXSTOREDB_SYMBOL_KIND_INSTANCEMETHOD;
  case SymbolKind::ClassMethod:
    return INDEXSTOREDB_SYMBOL_KIND_CLASSMETHOD;
  case SymbolKind::StaticMethod:
    return INDEXSTOREDB_SYMBOL_KIND_STATICMETHOD;
  case SymbolKind::InstanceProperty:
    return INDEXSTOREDB_SYMBOL_KIND_INSTANCEPROPERTY;
  case SymbolKind::ClassProperty:
    return INDEXSTOREDB_SYMBOL_KIND_CLASSPROPERTY;
  case SymbolKind::StaticProperty:
    return INDEXSTOREDB_SYMBOL_KIND_STATICPROPERTY;
  case SymbolKind::Constructor:
    return INDEXSTOREDB_SYMBOL_KIND_CONSTRUCTOR;
  case SymbolKind::Destructor:
    return INDEXSTOREDB_SYMBOL_KIND_DESTRUCTOR;
  case SymbolKind::ConversionFunction:
    return INDEXSTOREDB_SYMBOL_KIND_CONVERSIONFUNCTION;
  case SymbolKind::Concept:
    return INDEXSTOREDB_SYMBOL_KIND_CONCEPT;
  case SymbolKind::CommentTag:
    return INDEXSTOREDB_SYMBOL_KIND_COMMENTTAG;
  default:
    return INDEXSTOREDB_SYMBOL_KIND_UNKNOWN;
  }
}

const char *
indexstoredb_unit_info_main_file_path(indexstoredb_unit_info_t info) {
  auto obj = (const StoreUnitInfo *)info;
  return obj->MainFilePath.getPath().c_str();
}

const char *
indexstoredb_unit_info_unit_name(indexstoredb_unit_info_t info) {
  auto obj = (const StoreUnitInfo *)info;
  return obj->UnitName.c_str();
}

indexstoredb_symbol_provider_kind_t
indexstoredb_unit_info_symbol_provider_kind(_Nonnull indexstoredb_unit_info_t info) {
  auto obj = (const StoreUnitInfo *)info;
  if (!obj->SymProviderKind) {
    return INDEXSTOREDB_SYMBOL_PROVIDER_KIND_UNKNOWN;
  }
  switch (*obj->SymProviderKind) {
  case IndexStoreDB::SymbolProviderKind::Clang:
    return INDEXSTOREDB_SYMBOL_PROVIDER_KIND_CLANG;
  case IndexStoreDB::SymbolProviderKind::Swift:
    return INDEXSTOREDB_SYMBOL_PROVIDER_KIND_SWIFT;
  }
}

bool
indexstoredb_index_units_containing_file(
  indexstoredb_index_t index,
  const char *path,
  indexstoredb_unit_info_receiver receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachMainUnitContainingFile(path, [&](const StoreUnitInfo &unitInfo) -> bool {
    return receiver((indexstoredb_unit_info_receiver)&unitInfo);
  });
}

bool
indexstoredb_index_files_included_by_file(
  indexstoredb_index_t index,
  const char *path,
  indexstoredb_file_includes_receiver receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachFileIncludedByFile(path, [&](const CanonicalFilePathRef &sourcePath, unsigned line) -> bool {
    return receiver(sourcePath.getPath().str().c_str(), line);
  });
}

bool
indexstoredb_index_files_including_file(
  indexstoredb_index_t index,
  const char *path,
  indexstoredb_file_includes_receiver receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachFileIncludingFile(path, [&](const CanonicalFilePathRef &sourcePath, unsigned line) -> bool {
    return receiver(sourcePath.getPath().str().c_str(), line);
  });
}

bool
indexstoredb_index_includes_of_unit(
  indexstoredb_index_t index,
  const char *unitName,
  indexstoredb_unit_includes_receiver receiver)
{
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachIncludeOfUnit(unitName, [&](CanonicalFilePathRef sourcePath, CanonicalFilePathRef targetPath, unsigned line)->bool {
    return receiver(sourcePath.getPath().str().c_str(), targetPath.getPath().str().c_str(), line);
  });
}

bool
indexstoredb_index_unit_tests_referenced_by_main_files(
  _Nonnull indexstoredb_index_t index,
   const char *_Nonnull const *_Nonnull cMainFilePaths,
   size_t count,
  _Nonnull indexstoredb_symbol_occurrence_receiver_t receiver
) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  SmallVector<StringRef, 2> mainFilePaths;
  for (size_t i = 0; i < count; ++i) {
    mainFilePaths.push_back(cMainFilePaths[i]);
  }
  return obj->value->foreachUnitTestSymbolReferencedByMainFiles(mainFilePaths, [&](SymbolOccurrenceRef occur) -> bool {
    return receiver((indexstoredb_symbol_occurrence_t)occur.get());
  });
}

bool
indexstoredb_index_unit_tests(
  _Nonnull indexstoredb_index_t index,
  _Nonnull indexstoredb_symbol_occurrence_receiver_t receiver
) {
  auto obj = (Object<std::shared_ptr<IndexSystem>> *)index;
  return obj->value->foreachUnitTestSymbol([&](SymbolOccurrenceRef occur) -> bool {
    return receiver((indexstoredb_symbol_occurrence_t)occur.get());
  });
}

ObjectBase::~ObjectBase() {}
