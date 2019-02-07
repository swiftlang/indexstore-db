//===--- IndexStoreCXX.h - C++ wrapper for the Index Store C API. ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// A version of IndexStoreCXX.h that uses indexstore_functions.h
//
//===----------------------------------------------------------------------===//

#ifndef INDEXSTOREDB_INDEXSTORE_INDEXSTORECXX_H
#define INDEXSTOREDB_INDEXSTORE_INDEXSTORECXX_H

#include "indexstore/indexstore_functions.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include <ctime>

namespace indexstore {
  using llvm::ArrayRef;
  using llvm::Optional;
  using llvm::StringRef;

static inline StringRef stringFromIndexStoreStringRef(indexstore_string_ref_t str) {
  return StringRef(str.data, str.length);
}

class IndexStoreLibrary;
typedef std::shared_ptr<IndexStoreLibrary> IndexStoreLibraryRef;

class IndexStoreLibrary {
  indexstore_functions_t functions;
public:
  IndexStoreLibrary(indexstore_functions_t functions) : functions(functions) {}

  const indexstore_functions_t &api() const { return functions; }
};

template<typename FnT, typename ...Params>
static inline auto functionPtrFromFunctionRef(void *ctx, Params ...params)
    -> decltype((*(FnT *)ctx)(std::forward<Params>(params)...)) {
  auto fn = (FnT *)ctx;
  return (*fn)(std::forward<Params>(params)...);
}

class IndexRecordSymbol {
  indexstore_symbol_t obj;
  IndexStoreLibraryRef lib;
  friend class IndexRecordReader;

public:
  IndexRecordSymbol(indexstore_symbol_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

  indexstore_symbol_language_t getLanguage() {
    return lib->api().symbol_get_language(obj);
  }
  indexstore_symbol_kind_t getKind() { return lib->api().symbol_get_kind(obj); }
  indexstore_symbol_subkind_t getSubKind() { return lib->api().symbol_get_subkind(obj); }
  uint64_t getProperties() {
    return lib->api().symbol_get_properties(obj);
  }
  uint64_t getRoles() { return lib->api().symbol_get_roles(obj); }
  uint64_t getRelatedRoles() { return lib->api().symbol_get_related_roles(obj); }
  StringRef getName() { return stringFromIndexStoreStringRef(lib->api().symbol_get_name(obj)); }
  StringRef getUSR() { return stringFromIndexStoreStringRef(lib->api().symbol_get_usr(obj)); }
  StringRef getCodegenName() { return stringFromIndexStoreStringRef(lib->api().symbol_get_codegen_name(obj)); }
};

class IndexSymbolRelation {
  indexstore_symbol_relation_t obj;
  IndexStoreLibraryRef lib;

public:
  IndexSymbolRelation(indexstore_symbol_relation_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

  uint64_t getRoles() { return lib->api().symbol_relation_get_roles(obj); }
  IndexRecordSymbol getSymbol() { return {lib->api().symbol_relation_get_symbol(obj), lib}; }
};

class IndexRecordOccurrence {
  indexstore_occurrence_t obj;
  IndexStoreLibraryRef lib;

public:
  IndexRecordOccurrence(indexstore_occurrence_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

  IndexRecordSymbol getSymbol() { return {lib->api().occurrence_get_symbol(obj), lib}; }
  uint64_t getRoles() { return lib->api().occurrence_get_roles(obj); }

  bool foreachRelation(llvm::function_ref<bool(IndexSymbolRelation)> receiver) {
    auto forwarder = [&](indexstore_symbol_relation_t sym_rel) -> bool {
      return receiver({sym_rel, lib});
    };
    return lib->api().occurrence_relations_apply_f(obj, &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }

  std::pair<unsigned, unsigned> getLineCol() {
    unsigned line, col;
    lib->api().occurrence_get_line_col(obj, &line, &col);
    return std::make_pair(line, col);
  }
};

class IndexStore;
typedef std::shared_ptr<IndexStore> IndexStoreRef;

class IndexStore {
  indexstore_t obj;
  IndexStoreLibraryRef library;
  friend class IndexRecordReader;
  friend class IndexUnitReader;

public:
  IndexStore(StringRef path, IndexStoreLibraryRef library, std::string &error) : library(std::move(library)) {
    llvm::SmallString<64> buf = path;
    indexstore_error_t c_err = nullptr;
    obj = api().store_create(buf.c_str(), &c_err);
    if (c_err) {
      error = api().error_get_description(c_err);
      api().error_dispose(c_err);
    }
  }

  IndexStore(IndexStore &&other) : obj(other.obj) {
    other.obj = nullptr;
  }

  ~IndexStore() {
    api().store_dispose(obj);
  }

  static IndexStoreRef create(StringRef path, IndexStoreLibraryRef library, std::string &error) {
    auto storeRef = std::make_shared<IndexStore>(path, std::move(library), error);
    if (storeRef->isInvalid())
      return nullptr;
    return storeRef;
  }

  const indexstore_functions_t &api() const { return library->api(); }

  unsigned formatVersion() {
    return api().format_version();
  }

  bool isValid() const { return obj; }
  bool isInvalid() const { return !isValid(); }
  explicit operator bool() const { return isValid(); }

  bool foreachUnit(bool sorted, llvm::function_ref<bool(StringRef unitName)> receiver) {
    auto forwarder = [&](indexstore_string_ref_t unit_name) -> bool {
      return receiver(stringFromIndexStoreStringRef(unit_name));
    };
    return api().store_units_apply_f(obj, sorted, &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }

  class UnitEvent {
    indexstore_unit_event_t obj;
    IndexStoreLibraryRef lib;
  public:
    UnitEvent(indexstore_unit_event_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

    enum class Kind {
      Added,
      Removed,
      Modified,
      DirectoryDeleted,
    };
    Kind getKind() const {
      indexstore_unit_event_kind_t c_k = lib->api().unit_event_get_kind(obj);
      Kind K;
      switch (c_k) {
      case INDEXSTORE_UNIT_EVENT_ADDED: K = Kind::Added; break;
      case INDEXSTORE_UNIT_EVENT_REMOVED: K = Kind::Removed; break;
      case INDEXSTORE_UNIT_EVENT_MODIFIED: K = Kind::Modified; break;
      case INDEXSTORE_UNIT_EVENT_DIRECTORY_DELETED: K = Kind::DirectoryDeleted; break;
      }
      return K;
    }

    StringRef getUnitName() const {
      return stringFromIndexStoreStringRef(lib->api().unit_event_get_unit_name(obj));
    }

    timespec getModificationTime() const { return lib->api().unit_event_get_modification_time(obj); }
  };

  class UnitEventNotification {
    indexstore_unit_event_notification_t obj;
    IndexStoreLibraryRef lib;
  public:
    UnitEventNotification(indexstore_unit_event_notification_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

    bool isInitial() const { return lib->api().unit_event_notification_is_initial(obj); }
    size_t getEventsCount() const { return lib->api().unit_event_notification_get_events_count(obj); }
    UnitEvent getEvent(size_t index) const {
      return UnitEvent{lib->api().unit_event_notification_get_event(obj, index), lib};
    }
  };

  typedef std::function<void(UnitEventNotification)> UnitEventHandler;
  typedef std::function<void(indexstore_unit_event_notification_t)> RawUnitEventHandler;

  void setUnitEventHandler(UnitEventHandler handler) {
    auto localLib = std::weak_ptr<IndexStoreLibrary>(library);
    if (!handler) {
      api().store_set_unit_event_handler_f(obj, nullptr, nullptr, nullptr);
      return;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++14-extensions"
    auto fnPtr = new RawUnitEventHandler([handler, localLib=std::move(localLib)](
        indexstore_unit_event_notification_t evt_note) {
      if (auto lib = localLib.lock()) {
        handler(UnitEventNotification(evt_note, lib));
      }
    });
#pragma clang diagnostic pop
    api().store_set_unit_event_handler_f(obj, fnPtr, event_handler, event_handler_finalizer);
  }

private:
  static void event_handler(void *ctx, indexstore_unit_event_notification_t evt) {
    auto fnPtr = (RawUnitEventHandler*)ctx;
    (*fnPtr)(evt);
  }
  static void event_handler_finalizer(void *ctx) {
    auto fnPtr = (RawUnitEventHandler*)ctx;
    delete fnPtr;
  }

public:
  bool startEventListening(bool waitInitialSync, std::string &error) {
    indexstore_unit_event_listen_options_t opts;
    opts.wait_initial_sync = waitInitialSync;
    indexstore_error_t c_err = nullptr;
    bool ret = api().store_start_unit_event_listening(obj, &opts, sizeof(opts), &c_err);
    if (c_err) {
      error = api().error_get_description(c_err);
      api().error_dispose(c_err);
    }
    return ret;
  }

  void stopEventListening() {
    return api().store_stop_unit_event_listening(obj);
  }

  void discardUnit(StringRef UnitName) {
    llvm::SmallString<64> buf = UnitName;
    api().store_discard_unit(obj, buf.c_str());
  }

  void discardRecord(StringRef RecordName) {
    llvm::SmallString<64> buf = RecordName;
    api().store_discard_record(obj, buf.c_str());
  }

  void getUnitNameFromOutputPath(StringRef outputPath, llvm::SmallVectorImpl<char> &nameBuf) {
    llvm::SmallString<256> buf = outputPath;
    llvm::SmallString<64> unitName;
    unitName.resize(64);
    size_t nameLen = api().store_get_unit_name_from_output_path(obj, buf.c_str(), unitName.data(), unitName.size());
    if (nameLen+1 > unitName.size()) {
      unitName.resize(nameLen+1);
      api().store_get_unit_name_from_output_path(obj, buf.c_str(), unitName.data(), unitName.size());
    }
    nameBuf.append(unitName.begin(), unitName.begin()+nameLen);
  }

  llvm::Optional<timespec>
  getUnitModificationTime(StringRef unitName, std::string &error) {
    llvm::SmallString<64> buf = unitName;
    int64_t seconds, nanoseconds;
    indexstore_error_t c_err = nullptr;
    bool err = api().store_get_unit_modification_time(obj, buf.c_str(),
      &seconds, &nanoseconds, &c_err);
    if (err && c_err) {
      error = api().error_get_description(c_err);
      api().error_dispose(c_err);
      return llvm::None;
    }
    timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = nanoseconds;
    return ts;
  }

  void purgeStaleData() {
    api().store_purge_stale_data(obj);
  }
};

class IndexRecordReader {
  indexstore_record_reader_t obj;
  IndexStoreLibraryRef lib;

public:
  IndexRecordReader(IndexStore &store, StringRef recordName, std::string &error) : lib(store.library) {
    llvm::SmallString<64> buf = recordName;
    indexstore_error_t c_err = nullptr;
    obj = lib->api().record_reader_create(store.obj, buf.c_str(), &c_err);
    if (c_err) {
      error = lib->api().error_get_description(c_err);
      lib->api().error_dispose(c_err);
    }
  }

  IndexRecordReader(IndexRecordReader &&other) : obj(other.obj) {
    other.obj = nullptr;
  }

  ~IndexRecordReader() {
    lib->api().record_reader_dispose(obj);
  }

  bool isValid() const { return obj; }
  bool isInvalid() const { return !isValid(); }
  explicit operator bool() const { return isValid(); }

  /// Goes through and passes record decls, after filtering using a \c Checker
  /// function.
  ///
  /// Resulting decls can be used as filter for \c foreachOccurrence. This
  /// allows allocating memory only for the record decls that the caller is
  /// interested in.
  bool searchSymbols(llvm::function_ref<bool(IndexRecordSymbol, bool &stop)> filter,
                     llvm::function_ref<void(IndexRecordSymbol)> receiver) {
    auto forwarder_filter = [&](indexstore_symbol_t symbol, bool *stop) -> bool {
      return filter({symbol, lib}, *stop);
    };
    auto forwarder_receiver = [&](indexstore_symbol_t symbol) {
      receiver({symbol, lib});
    };
    return lib->api().record_reader_search_symbols_f(obj, &forwarder_filter, functionPtrFromFunctionRef<decltype(forwarder_filter)>,
                                                     &forwarder_receiver, functionPtrFromFunctionRef<decltype(forwarder_receiver)>);
  }

  bool foreachSymbol(bool noCache, llvm::function_ref<bool(IndexRecordSymbol)> receiver) {
    auto forwarder = [&](indexstore_symbol_t sym) -> bool {
      return receiver({sym, lib});
    };
    return lib->api().record_reader_symbols_apply_f(obj, noCache, &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }

  /// \param DeclsFilter if non-empty indicates the list of decls that we want
  /// to get occurrences for. An empty array indicates that we want occurrences
  /// for all decls.
  /// \param RelatedDeclsFilter Same as \c DeclsFilter but for related decls.
  bool foreachOccurrence(ArrayRef<IndexRecordSymbol> symbolsFilter,
                         ArrayRef<IndexRecordSymbol> relatedSymbolsFilter,
              llvm::function_ref<bool(IndexRecordOccurrence)> receiver) {
    llvm::SmallVector<indexstore_symbol_t, 16> c_symbolsFilter;
    c_symbolsFilter.reserve(symbolsFilter.size());
    for (IndexRecordSymbol sym : symbolsFilter) {
      c_symbolsFilter.push_back(sym.obj);
    }
    llvm::SmallVector<indexstore_symbol_t, 16> c_relatedSymbolsFilter;
    c_relatedSymbolsFilter.reserve(relatedSymbolsFilter.size());
    for (IndexRecordSymbol sym : relatedSymbolsFilter) {
      c_relatedSymbolsFilter.push_back(sym.obj);
    }
    auto forwarder = [&](indexstore_occurrence_t occur) -> bool {
      return receiver({occur, lib});
    };
    return lib->api().record_reader_occurrences_of_symbols_apply_f(obj,
                                c_symbolsFilter.data(), c_symbolsFilter.size(),
                                c_relatedSymbolsFilter.data(),
                                c_relatedSymbolsFilter.size(),
                                &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }

  bool foreachOccurrence(
              llvm::function_ref<bool(IndexRecordOccurrence)> receiver) {
    auto forwarder = [&](indexstore_occurrence_t occur) -> bool {
      return receiver({occur, lib});
    };
    return lib->api().record_reader_occurrences_apply_f(obj, &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }

  bool foreachOccurrenceInLineRange(unsigned lineStart, unsigned lineEnd,
              llvm::function_ref<bool(IndexRecordOccurrence)> receiver) {
    auto forwarder = [&](indexstore_occurrence_t occur) -> bool {
      return receiver({occur, lib});
    };
    return lib->api().record_reader_occurrences_in_line_range_apply_f(obj,
                                                                      lineStart,
                                                                      lineEnd,
                                         &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }
};

class IndexUnitDependency {
  indexstore_unit_dependency_t obj;
  IndexStoreLibraryRef lib;
  friend class IndexUnitReader;

public:
  IndexUnitDependency(indexstore_unit_dependency_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

  enum class DependencyKind {
    Unit,
    Record,
    File,
  };
  DependencyKind getKind() {
    switch (lib->api().unit_dependency_get_kind(obj)) {
    case INDEXSTORE_UNIT_DEPENDENCY_UNIT: return DependencyKind::Unit;
    case INDEXSTORE_UNIT_DEPENDENCY_RECORD: return DependencyKind::Record;
    case INDEXSTORE_UNIT_DEPENDENCY_FILE: return DependencyKind::File;
    }
  }
  bool isSystem() { return lib->api().unit_dependency_is_system(obj); }
  StringRef getName() { return stringFromIndexStoreStringRef(lib->api().unit_dependency_get_name(obj)); }
  StringRef getFilePath() { return stringFromIndexStoreStringRef(lib->api().unit_dependency_get_filepath(obj)); }
  StringRef getModuleName() { return stringFromIndexStoreStringRef(lib->api().unit_dependency_get_modulename(obj)); }

};

class IndexUnitInclude {
  indexstore_unit_include_t obj;
  IndexStoreLibraryRef lib;
  friend class IndexUnitReader;

public:
  IndexUnitInclude(indexstore_unit_include_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(std::move(lib)) {}

  StringRef getSourcePath() {
    return stringFromIndexStoreStringRef(lib->api().unit_include_get_source_path(obj));
  }
  StringRef getTargetPath() {
    return stringFromIndexStoreStringRef(lib->api().unit_include_get_target_path(obj));
  }
  unsigned getSourceLine() {
    return lib->api().unit_include_get_source_line(obj);
  }
};

class IndexUnitReader {
  indexstore_unit_reader_t obj;
  IndexStoreLibraryRef lib;

public:
  IndexUnitReader(IndexStore &store, StringRef unitName, std::string &error) : lib(store.library) {
    llvm::SmallString<64> buf = unitName;
    indexstore_error_t c_err = nullptr;
    obj = lib->api().unit_reader_create(store.obj, buf.c_str(), &c_err);
    if (c_err) {
      error = lib->api().error_get_description(c_err);
      lib->api().error_dispose(c_err);
    }
  }

  IndexUnitReader(IndexUnitReader &&other) : obj(other.obj) {
    other.obj = nullptr;
  }

  ~IndexUnitReader() {
    lib->api().unit_reader_dispose(obj);
  }

  bool isValid() const { return obj; }
  bool isInvalid() const { return !isValid(); }
  explicit operator bool() const { return isValid(); }

  StringRef getProviderIdentifier() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_provider_identifier(obj));
  }
  StringRef getProviderVersion() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_provider_version(obj));
  }

  timespec getModificationTime() {
    int64_t seconds, nanoseconds;
    lib->api().unit_reader_get_modification_time(obj, &seconds, &nanoseconds);
    timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = nanoseconds;
    return ts;
  }

  bool isSystemUnit() { return lib->api().unit_reader_is_system_unit(obj); }
  bool isModuleUnit() { return lib->api().unit_reader_is_module_unit(obj); }
  bool isDebugCompilation() { return lib->api().unit_reader_is_debug_compilation(obj); }
  bool hasMainFile() { return lib->api().unit_reader_has_main_file(obj); }

  StringRef getMainFilePath() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_main_file(obj));
  }
  StringRef getModuleName() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_module_name(obj));
  }
  StringRef getWorkingDirectory() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_working_dir(obj));
  }
  StringRef getOutputFile() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_output_file(obj));
  }
  StringRef getSysrootPath() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_sysroot_path(obj));
  }
  StringRef getTarget() {
    return stringFromIndexStoreStringRef(lib->api().unit_reader_get_target(obj));
  }

  bool foreachDependency(llvm::function_ref<bool(IndexUnitDependency)> receiver) {
    auto forwarder = [&](indexstore_unit_dependency_t dep) -> bool {
      return receiver({dep, lib});
    };
    return lib->api().unit_reader_dependencies_apply_f(obj, &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);;
  }

  bool foreachInclude(llvm::function_ref<bool(IndexUnitInclude)> receiver) {
    auto forwarder = [&](indexstore_unit_include_t inc) -> bool {
      return receiver({inc, lib});
    };
    return lib->api().unit_reader_includes_apply_f(obj, &forwarder, functionPtrFromFunctionRef<decltype(forwarder)>);
  }
};

} // namespace indexstore

#endif
