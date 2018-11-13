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

template<typename Ret, typename ...Params>
static inline Ret functionPtrFromFunctionRef(void *ctx, Params ...params) {
  auto fn = (llvm::function_ref<Ret(Params...)> *)ctx;
  return (*fn)(std::forward<Params>(params)...);
}

class IndexRecordSymbol {
  indexstore_symbol_t obj;
  IndexStoreLibraryRef lib;
  friend class IndexRecordReader;

public:
  IndexRecordSymbol(indexstore_symbol_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

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
  IndexSymbolRelation(indexstore_symbol_relation_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

  uint64_t getRoles() { return lib->api().symbol_relation_get_roles(obj); }
  IndexRecordSymbol getSymbol() { return {lib->api().symbol_relation_get_symbol(obj), lib}; }
};

class IndexRecordOccurrence {
  indexstore_occurrence_t obj;
  IndexStoreLibraryRef lib;

public:
  IndexRecordOccurrence(indexstore_occurrence_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

  IndexRecordSymbol getSymbol() { return {lib->api().occurrence_get_symbol(obj), lib}; }
  uint64_t getRoles() { return lib->api().occurrence_get_roles(obj); }

  bool foreachRelation(llvm::function_ref<bool(IndexSymbolRelation)> receiver) {
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().occurrence_relations_apply(obj, ^bool(indexstore_symbol_relation_t sym_rel) {
      return receiver({sym_rel, lib});
    });
#else
    return lib->api().occurrence_relations_apply_f(obj, &receiver, functionPtrFromFunctionRef);
#endif
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
#if INDEXSTORE_HAS_BLOCKS
    return api().store_units_apply(obj, sorted, ^bool(indexstore_string_ref_t unit_name) {
      return receiver(stringFromIndexStoreStringRef(unit_name));
    });
#else
    return api().store_units_apply_f(obj, sorted, &receiver, functionPtrFromFunctionRef);
#endif
  }

  class UnitEvent {
    indexstore_unit_event_t obj;
    IndexStoreLibraryRef lib;
  public:
    UnitEvent(indexstore_unit_event_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

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
    UnitEventNotification(indexstore_unit_event_notification_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

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
#if INDEXSTORE_HAS_BLOCKS
    if (!handler) {
      api().store_set_unit_event_handler(obj, nullptr);
      return;
    }

    api().store_set_unit_event_handler(obj, ^(indexstore_unit_event_notification_t evt_note) {
      if (auto lib = localLib.lock()) {
        handler(UnitEventNotification(evt_note, lib));
      }
    });
#else
    if (!handler) {
      api().store_set_unit_event_handler_f(obj, nullptr, nullptr, nullptr);
      return;
    }

    auto fnPtr = new RawUnitEventHandler([handler, localLib=std::move(localLib)](
        indexstore_unit_event_notification_t evt_note) {
      if (auto lib = localLib.lock()) {
        handler(UnitEventNotification(evt_note, lib));
      }
    });
    api().store_set_unit_event_handler_f(obj, fnPtr, event_handler, event_handler_finalizer);
#endif
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
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().record_reader_search_symbols(obj, ^bool(indexstore_symbol_t symbol, bool *stop) {
      return filter({symbol, lib}, *stop);
    }, ^(indexstore_symbol_t symbol) {
      receiver({symbol, lib});
    });
#else
    return lib->api().record_reader_search_symbols_f(obj, &filter, functionPtrFromFunctionRef,
                                                     &receiver, functionPtrFromFunctionRef);
#endif
  }

  bool foreachSymbol(bool noCache, llvm::function_ref<bool(IndexRecordSymbol)> receiver) {
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().record_reader_symbols_apply(obj, noCache, ^bool(indexstore_symbol_t sym) {
      return receiver({sym, lib});
    });
#else
    return lib->api().record_reader_symbols_apply_f(obj, noCache, &receiver, functionPtrFromFunctionRef);
#endif
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
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().record_reader_occurrences_of_symbols_apply(obj,
                                c_symbolsFilter.data(), c_symbolsFilter.size(),
                                c_relatedSymbolsFilter.data(),
                                c_relatedSymbolsFilter.size(),
                                ^bool(indexstore_occurrence_t occur) {
                                  return receiver({occur, lib});
                                });
#else
    return lib->api().record_reader_occurrences_of_symbols_apply_f(obj,
                                c_symbolsFilter.data(), c_symbolsFilter.size(),
                                c_relatedSymbolsFilter.data(),
                                c_relatedSymbolsFilter.size(),
                                &receiver, functionPtrFromFunctionRef);
#endif
  }

  bool foreachOccurrence(
              llvm::function_ref<bool(IndexRecordOccurrence)> receiver) {
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().record_reader_occurrences_apply(obj, ^bool(indexstore_occurrence_t occur) {
      return receiver({occur, lib});
    });
#else
    return lib->api().record_reader_occurrences_apply_f(obj, &receiver, functionPtrFromFunctionRef);
#endif
  }

  bool foreachOccurrenceInLineRange(unsigned lineStart, unsigned lineEnd,
              llvm::function_ref<bool(IndexRecordOccurrence)> receiver) {
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().record_reader_occurrences_in_line_range_apply(obj,
                                                                    lineStart,
                                                                    lineEnd,
                                          ^bool(indexstore_occurrence_t occur) {
      return receiver({occur, lib});
    });
#else
    return lib->api().record_reader_occurrences_in_line_range_apply_f(obj,
                                                                      lineStart,
                                                                      lineEnd,
                                         &receiver, functionPtrFromFunctionRef);
#endif
  }
};

class IndexUnitDependency {
  indexstore_unit_dependency_t obj;
  IndexStoreLibraryRef lib;
  friend class IndexUnitReader;

public:
  IndexUnitDependency(indexstore_unit_dependency_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

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
  IndexUnitInclude(indexstore_unit_include_t obj, IndexStoreLibraryRef lib) : obj(obj), lib(lib) {}

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
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().unit_reader_dependencies_apply(obj, ^bool(indexstore_unit_dependency_t dep) {
      return receiver({dep, lib});
    });
#else
    return lib->api().unit_reader_dependencies_apply_f(obj, &receiver, functionPtrFromFunctionRef);;
#endif
  }

  bool foreachInclude(llvm::function_ref<bool(IndexUnitInclude)> receiver) {
#if INDEXSTORE_HAS_BLOCKS
    return lib->api().unit_reader_includes_apply(obj, ^bool(indexstore_unit_include_t inc) {
      return receiver({inc, lib});
    });
#else
    return lib->api().unit_reader_includes_apply_f(obj, &receiver, functionPtrFromFunctionRef);
#endif
  }
};

} // namespace indexstore

#endif
