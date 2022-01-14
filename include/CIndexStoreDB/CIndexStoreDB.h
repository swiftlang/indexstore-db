/*===--- CIndexStoreDB.h ------------------------------------------*- C -*-===//
 *
 * This source file is part of the Swift.org open source project
 *
 * Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
 * Licensed under Apache License v2.0 with Runtime Library Exception
 *
 * See https://swift.org/LICENSE.txt for license information
 * See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
 *
 *===----------------------------------------------------------------------===*/

#ifndef INDEXSTOREDB_INDEX_H
#define INDEXSTOREDB_INDEX_H

#include "indexstore/indexstore_functions.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
# define INDEXSTOREDB_BEGIN_DECLS  extern "C" {
# define INDEXSTOREDB_END_DECLS    }
#else
# define INDEXSTOREDB_BEGIN_DECLS
# define INDEXSTOREDB_END_DECLS
#endif

#ifndef INDEXSTOREDB_PUBLIC
# if defined(_WIN32) && defined(_WINDLL)
#   if defined(CIndexStoreDB_EXPORTS)
#     define INDEXSTOREDB_PUBLIC __declspec(dllexport)
#   else
#     define INDEXSTOREDB_PUBLIC __declspec(dllimport)
#   endif
# else
#   define INDEXSTOREDB_PUBLIC
# endif
#endif

#ifndef __has_feature
# define __has_feature(x) 0
#endif

#if !__has_feature(nullability)
# define _Nullable
# define _Nonnull
#endif

INDEXSTOREDB_BEGIN_DECLS

typedef void *indexstoredb_object_t;
typedef indexstoredb_object_t indexstoredb_index_t;
typedef indexstoredb_object_t indexstoredb_indexstore_library_t;

typedef void *indexstoredb_symbol_t;
typedef void *indexstoredb_symbol_occurrence_t;
typedef void *indexstoredb_error_t;
typedef void *indexstoredb_symbol_location_t;
typedef void *indexstoredb_symbol_relation_t;
typedef void *indexstoredb_unit_info_t;

typedef enum {
  INDEXSTOREDB_SYMBOL_ROLE_DECLARATION = 1 << 0,
  INDEXSTOREDB_SYMBOL_ROLE_DEFINITION  = 1 << 1,
  INDEXSTOREDB_SYMBOL_ROLE_REFERENCE   = 1 << 2,
  INDEXSTOREDB_SYMBOL_ROLE_READ        = 1 << 3,
  INDEXSTOREDB_SYMBOL_ROLE_WRITE       = 1 << 4,
  INDEXSTOREDB_SYMBOL_ROLE_CALL        = 1 << 5,
  INDEXSTOREDB_SYMBOL_ROLE_DYNAMIC     = 1 << 6,
  INDEXSTOREDB_SYMBOL_ROLE_ADDRESSOF   = 1 << 7,
  INDEXSTOREDB_SYMBOL_ROLE_IMPLICIT    = 1 << 8,

  // Relation roles.
  INDEXSTOREDB_SYMBOL_ROLE_REL_CHILDOF     = 1 << 9,
  INDEXSTOREDB_SYMBOL_ROLE_REL_BASEOF      = 1 << 10,
  INDEXSTOREDB_SYMBOL_ROLE_REL_OVERRIDEOF  = 1 << 11,
  INDEXSTOREDB_SYMBOL_ROLE_REL_RECEIVEDBY  = 1 << 12,
  INDEXSTOREDB_SYMBOL_ROLE_REL_CALLEDBY    = 1 << 13,
  INDEXSTOREDB_SYMBOL_ROLE_REL_EXTENDEDBY  = 1 << 14,
  INDEXSTOREDB_SYMBOL_ROLE_REL_ACCESSOROF  = 1 << 15,
  INDEXSTOREDB_SYMBOL_ROLE_REL_CONTAINEDBY = 1 << 16,
  INDEXSTOREDB_SYMBOL_ROLE_REL_IBTYPEOF    = 1 << 17,
  INDEXSTOREDB_SYMBOL_ROLE_REL_SPECIALIZATIONOF = 1 << 18,

  INDEXSTOREDB_SYMBOL_ROLE_CANONICAL = 1 << 63,
} indexstoredb_symbol_role_t;

typedef enum {
  INDEXSTOREDB_SYMBOL_KIND_UNKNOWN = 0,
  INDEXSTOREDB_SYMBOL_KIND_MODULE = 1,
  INDEXSTOREDB_SYMBOL_KIND_NAMESPACE = 2,
  INDEXSTOREDB_SYMBOL_KIND_NAMESPACEALIAS = 3,
  INDEXSTOREDB_SYMBOL_KIND_MACRO = 4,
  INDEXSTOREDB_SYMBOL_KIND_ENUM = 5,
  INDEXSTOREDB_SYMBOL_KIND_STRUCT = 6,
  INDEXSTOREDB_SYMBOL_KIND_CLASS = 7,
  INDEXSTOREDB_SYMBOL_KIND_PROTOCOL = 8,
  INDEXSTOREDB_SYMBOL_KIND_EXTENSION = 9,
  INDEXSTOREDB_SYMBOL_KIND_UNION = 10,
  INDEXSTOREDB_SYMBOL_KIND_TYPEALIAS = 11,
  INDEXSTOREDB_SYMBOL_KIND_FUNCTION = 12,
  INDEXSTOREDB_SYMBOL_KIND_VARIABLE = 13,
  INDEXSTOREDB_SYMBOL_KIND_FIELD = 14,
  INDEXSTOREDB_SYMBOL_KIND_ENUMCONSTANT = 15,
  INDEXSTOREDB_SYMBOL_KIND_INSTANCEMETHOD = 16,
  INDEXSTOREDB_SYMBOL_KIND_CLASSMETHOD = 17,
  INDEXSTOREDB_SYMBOL_KIND_STATICMETHOD = 18,
  INDEXSTOREDB_SYMBOL_KIND_INSTANCEPROPERTY = 19,
  INDEXSTOREDB_SYMBOL_KIND_CLASSPROPERTY = 20,
  INDEXSTOREDB_SYMBOL_KIND_STATICPROPERTY = 21,
  INDEXSTOREDB_SYMBOL_KIND_CONSTRUCTOR = 22,
  INDEXSTOREDB_SYMBOL_KIND_DESTRUCTOR = 23,
  INDEXSTOREDB_SYMBOL_KIND_CONVERSIONFUNCTION = 24,
  INDEXSTOREDB_SYMBOL_KIND_PARAMETER = 25,
  INDEXSTOREDB_SYMBOL_KIND_USING = 26,

  INDEXSTOREDB_SYMBOL_KIND_COMMENTTAG = 1000,
} indexstoredb_symbol_kind_t;

typedef enum {
  INDEXSTOREDB_SYMBOL_PROPERTY_GENERIC                          = 1 << 0,
  INDEXSTOREDB_SYMBOL_PROPERTY_TEMPLATE_PARTIAL_SPECIALIZATION  = 1 << 1,
  INDEXSTOREDB_SYMBOL_PROPERTY_TEMPLATE_SPECIALIZATION          = 1 << 2,
  INDEXSTOREDB_SYMBOL_PROPERTY_UNITTEST                         = 1 << 3,
  INDEXSTOREDB_SYMBOL_PROPERTY_IBANNOTATED                      = 1 << 4,
  INDEXSTOREDB_SYMBOL_PROPERTY_IBOUTLETCOLLECTION               = 1 << 5,
  INDEXSTOREDB_SYMBOL_PROPERTY_GKINSPECTABLE                    = 1 << 6,
  INDEXSTOREDB_SYMBOL_PROPERTY_LOCAL                            = 1 << 7,
  INDEXSTOREDB_SYMBOL_PROPERTY_PROTOCOL_INTERFACE               = 1 << 8,
  INDEXSTOREDB_SYMBOL_PROPERTY_SWIFT_ASYNC                      = 1 << 16,
} indexstoredb_symbol_property_t;

typedef enum {
  INDEXSTOREDB_EVENT_PROCESSING_ADDED_PENDING = 0,
  INDEXSTOREDB_EVENT_PROCESSING_COMPLETED = 1,
  INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE = 2,
} indexstoredb_delegate_event_kind_t;

typedef void *indexstoredb_delegate_event_t;

/// Returns true on success.
typedef _Nullable indexstoredb_indexstore_library_t(^indexstore_library_provider_t)(const char * _Nonnull);

/// Returns true to continue.
typedef bool(^indexstoredb_symbol_receiver_t)(_Nonnull indexstoredb_symbol_t);

/// Returns true to continue.
typedef bool(^indexstoredb_symbol_occurrence_receiver_t)(_Nonnull indexstoredb_symbol_occurrence_t);

/// Returns true to continue.
typedef bool(^indexstoredb_symbol_name_receiver)(const char *_Nonnull);

typedef void(^indexstoredb_delegate_event_receiver_t)(_Nonnull indexstoredb_delegate_event_t);

/// Returns true to continue.
typedef bool(^indexstoredb_unit_info_receiver)(_Nonnull indexstoredb_unit_info_t);

/// Returns true to continue.
typedef bool(^indexstoredb_file_includes_receiver)(const char *_Nonnull sourcePath, size_t line);

/// Returns true to continue.
typedef bool(^indexstoredb_unit_includes_receiver)(const char *_Nonnull sourcePath, const char *_Nonnull targetPath, size_t line);

/// Creates an index for the given raw index data in \p storePath.
///
/// The resulting index must be released using \c indexstoredb_release.
INDEXSTOREDB_PUBLIC _Nullable
indexstoredb_index_t
indexstoredb_index_create(const char * _Nonnull storePath,
                  const char * _Nonnull databasePath,
                  _Nonnull indexstore_library_provider_t libProvider,
                  _Nonnull indexstoredb_delegate_event_receiver_t delegate,
                  bool useExplicitOutputUnits,
                  bool wait,
                  bool readonly,
                  bool enableOutOfDateFileWatching,
                  bool listenToUnitEvents,
                  indexstoredb_error_t _Nullable * _Nullable);

/// Add an additional delegate to the given index.
INDEXSTOREDB_PUBLIC void
indexstoredb_index_add_delegate(_Nonnull indexstoredb_index_t index,
                                _Nonnull indexstoredb_delegate_event_receiver_t delegate);

/// Creates an indexstore library for the given library.
///
/// The resulting object must be released using \c indexstoredb_release.
INDEXSTOREDB_PUBLIC _Nullable
indexstoredb_indexstore_library_t
indexstoredb_load_indexstore_library(const char * _Nonnull dylibPath,
                             indexstoredb_error_t _Nullable * _Nullable);

/// *For Testing* Poll for any changes to index units and wait until they have been registered.
INDEXSTOREDB_PUBLIC void
indexstoredb_index_poll_for_unit_changes_and_wait(_Nonnull indexstoredb_index_t index, bool isInitialScan);

/// Add output filepaths for the set of unit files that index data should be loaded from.
/// Only has an effect if `useExplicitOutputUnits` was set to true for `indexstoredb_index_create`.
INDEXSTOREDB_PUBLIC void
indexstoredb_index_add_unit_out_file_paths(_Nonnull indexstoredb_index_t index,
                                           const char *_Nonnull const *_Nonnull paths,
                                           size_t count,
                                           bool waitForProcessing);

/// Remove output filepaths from the set of unit files that index data should be loaded from.
/// Only has an effect if `useExplicitOutputUnits` was set to true for `indexstoredb_index_create`.
INDEXSTOREDB_PUBLIC void
indexstoredb_index_remove_unit_out_file_paths(_Nonnull indexstoredb_index_t index,
                                              const char *_Nonnull const *_Nonnull paths,
                                              size_t count,
                                              bool waitForProcessing);

INDEXSTOREDB_PUBLIC
indexstoredb_delegate_event_kind_t
indexstoredb_delegate_event_get_kind(_Nonnull indexstoredb_delegate_event_t);

INDEXSTOREDB_PUBLIC
uint64_t indexstoredb_delegate_event_get_count(_Nonnull indexstoredb_delegate_event_t);

/// Valid only if the event kind is \p INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE, otherwise returns null.
/// The indexstoredb_unit_info_t pointer has the same lifetime as the \c indexstoredb_delegate_event_t
INDEXSTOREDB_PUBLIC _Nullable indexstoredb_unit_info_t
indexstoredb_delegate_event_get_outofdate_unit_info(_Nonnull indexstoredb_delegate_event_t);

/// Returns number of nanoseconds since clock's epoch.
/// Valid only if the event kind is \p INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE, otherwise returns 0.
INDEXSTOREDB_PUBLIC uint64_t
indexstoredb_delegate_event_get_outofdate_modtime(_Nonnull indexstoredb_delegate_event_t);

/// Valid only if the event kind is \p INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE, otherwise returns false.
INDEXSTOREDB_PUBLIC bool
indexstoredb_delegate_event_get_outofdate_is_synchronous(_Nonnull indexstoredb_delegate_event_t);

/// Valid only if the event kind is \p INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE, otherwise returns null.
/// The string has the same lifetime as the \c indexstoredb_delegate_event_t.
INDEXSTOREDB_PUBLIC const char * _Nullable
indexstoredb_delegate_event_get_outofdate_trigger_original_file(_Nonnull indexstoredb_delegate_event_t);

/// Valid only if the event kind is \p INDEXSTOREDB_EVENT_UNIT_OUT_OF_DATE, otherwise returns null.
/// The string has the same lifetime as the \c indexstoredb_delegate_event_t.
INDEXSTOREDB_PUBLIC const char * _Nullable
indexstoredb_delegate_event_get_outofdate_trigger_description(_Nonnull indexstoredb_delegate_event_t);

/// Iterates over each symbol occurrence matching the given \p usr and \p roles.
///
/// The occurrence passed to the receiver is only valid for the duration of the
/// receiver call.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_symbol_occurrences_by_usr(
    _Nonnull indexstoredb_index_t index,
    const char *_Nonnull usr,
    uint64_t roles,
    _Nonnull indexstoredb_symbol_occurrence_receiver_t);

/// Iterates over each symbol occurrence related to the \p usr with \p roles.
///
/// The occurrence passed to the receiver is only valid for the duration of the
/// receiver call.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_related_symbol_occurrences_by_usr(
    _Nonnull indexstoredb_index_t index,
    const char *_Nonnull usr,
    uint64_t roles,
    _Nonnull indexstoredb_symbol_occurrence_receiver_t);

/// Iterates over all the symbols contained in \p path
/// 
/// The symbol passed to the receiver is only valid for the duration of the
/// receiver call.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_symbols_contained_in_file_path(_Nonnull indexstoredb_index_t index,
                                                  const char *_Nonnull path,
                                                  _Nonnull indexstoredb_symbol_receiver_t);

/// Returns the USR of the given symbol.
///
/// The string has the same lifetime as the \c indexstoredb_symbol_t.
INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_usr(_Nonnull indexstoredb_symbol_t);

/// Returns the name of the given symbol.
///
/// The string has the same lifetime as the \c indexstoredb_symbol_t.
INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_name(_Nonnull indexstoredb_symbol_t);

/// Returns the properties of the given symbol.
INDEXSTOREDB_PUBLIC uint64_t
indexstoredb_symbol_properties(_Nonnull indexstoredb_symbol_t);

/// Returns the symbol of the given symbol occurrence.
///
/// The symbol has the same lifetime as the \c indexstoredb_symbol_occurrence_t.
INDEXSTOREDB_PUBLIC
_Nonnull indexstoredb_symbol_t
indexstoredb_symbol_occurrence_symbol(_Nonnull indexstoredb_symbol_occurrence_t);

/// Returns the roles of the given symbol occurrence.
INDEXSTOREDB_PUBLIC uint64_t
indexstoredb_symbol_occurrence_roles(_Nonnull indexstoredb_symbol_occurrence_t);

/// Returns the location of the given symbol occurrence.
///
/// The location has the same lifetime as the \c indexstoredb_symbol_occurrence_t.
INDEXSTOREDB_PUBLIC _Nonnull
indexstoredb_symbol_location_t
indexstoredb_symbol_occurrence_location(_Nonnull indexstoredb_symbol_occurrence_t);

/// Returns the path of the given symbol location.
///
/// The string has the same lifetime as the \c indexstoredb_symbol_location_t.
INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_location_path(_Nonnull indexstoredb_symbol_location_t);

/// Returns the module name of the given symbol location.
///
/// The string has the same lifetime as the \c indexstoredb_symbol_location_t.
INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_location_module_name(_Nonnull indexstoredb_symbol_location_t);

/// Returns whether the given symbol location is a system location.
INDEXSTOREDB_PUBLIC bool
indexstoredb_symbol_location_is_system(_Nonnull indexstoredb_symbol_location_t);

/// Returns the one-based line number of the given symbol location.
INDEXSTOREDB_PUBLIC int
indexstoredb_symbol_location_line(_Nonnull indexstoredb_symbol_location_t);

/// Returns the one-based UTF-8 column index of the given symbol location.
INDEXSTOREDB_PUBLIC int
indexstoredb_symbol_location_column_utf8(_Nonnull indexstoredb_symbol_location_t);

/// Retains the given \c indexstoredb_object_t and returns it.
INDEXSTOREDB_PUBLIC _Nonnull
indexstoredb_object_t
indexstoredb_retain(_Nonnull indexstoredb_object_t);

/// Releases the given \c indexstoredb_object_t.
INDEXSTOREDB_PUBLIC void
indexstoredb_release(_Nonnull indexstoredb_object_t);

/// Returns the string describing the given error.
///
/// The string has the same lifetime as the \c indexstoredb_error_t.
INDEXSTOREDB_PUBLIC const char * _Nonnull
indexstoredb_error_get_description(_Nonnull indexstoredb_error_t);

/// Destroys the given error.
INDEXSTOREDB_PUBLIC void
indexstoredb_error_dispose(_Nullable indexstoredb_error_t);

/// Iterates over the name of every symbol in the index.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param receiver A function to be called for each symbol. The string pointer is only valid for
/// the duration of the call. The function should return a true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_symbol_names(_Nonnull indexstoredb_index_t index, _Nonnull indexstoredb_symbol_name_receiver);

/// Iterates over every canonical symbol that matches the string.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param symbolName The name of the symbol whose canonical occurence should be found.
/// \param receiver A function to be called for each canonical occurence.
/// The canonical symbol occurrence will be passed in to this function. It is valid only for the
/// duration of the call. The function should return true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_canonical_symbol_occurences_by_name(
    indexstoredb_index_t _Nonnull index,
    const char *_Nonnull symbolName,
    indexstoredb_symbol_occurrence_receiver_t _Nonnull receiver
);

/// Iterates over every canonical symbol that matches the pattern.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param anchorStart When true, symbol names should only be considered matching when the first characters of the symbol name match the pattern.
/// \param anchorEnd When true, symbol names should only be considered matching when the first characters of the symbol name match the pattern.
/// \param subsequence When true, symbols will be matched even if the pattern is not matched contiguously.
/// \param ignoreCase When true, symbols may be returned even if the case of letters does not match the pattern.
/// \param receiver A function to be called for each canonical occurence that matches the pattern.
/// It is valid only for the duration of the call. The function should return true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_canonical_symbol_occurences_containing_pattern(
    _Nonnull indexstoredb_index_t index,
    const char *_Nonnull pattern,
    bool anchorStart,
    bool anchorEnd,
    bool subsequence,
    bool ignoreCase,
    _Nonnull indexstoredb_symbol_occurrence_receiver_t receiver);

/// Returns the set of roles of the given symbol relation.
INDEXSTOREDB_PUBLIC uint64_t
indexstoredb_symbol_relation_get_roles(_Nonnull  indexstoredb_symbol_relation_t);

/// Returns the symbol of the given symbol relation.
///
/// The symbol has the same lifetime as the \c indexstoredb_symbol_relation_t.
INDEXSTOREDB_PUBLIC _Nonnull indexstoredb_symbol_t
indexstoredb_symbol_relation_get_symbol(_Nonnull indexstoredb_symbol_relation_t);

/// Iterates over the relations of the given symbol occurrence.
///
/// The relations are owned by the occurrence and shall not be used after the occurrence is freed.
///
/// \param occurrence The symbol occurrence that whose relations should be found.
/// \param applier The function that should be performed on each symbol relation.
/// The function should return a boolean indicating whether the looping should continue.
INDEXSTOREDB_PUBLIC bool
indexstoredb_symbol_occurrence_relations(_Nonnull indexstoredb_symbol_occurrence_t,
                                         bool(^ _Nonnull applier)(indexstoredb_symbol_relation_t _Nonnull ));

/// Returns the kind of the given symbol.
INDEXSTOREDB_PUBLIC indexstoredb_symbol_kind_t
indexstoredb_symbol_kind(_Nonnull indexstoredb_symbol_t);

/// Returns the main file path of a unit info object.
///
/// The main file is typically the one that e.g. a build system would have explicit knowledge of.
INDEXSTOREDB_PUBLIC const char *_Nonnull
indexstoredb_unit_info_main_file_path(_Nonnull indexstoredb_unit_info_t);

/// Returns the unit name of a unit info object.
INDEXSTOREDB_PUBLIC const char *_Nonnull
indexstoredb_unit_info_unit_name(_Nonnull indexstoredb_unit_info_t);

/// Iterates over the compilation units that contain \p path and return their units.
///
/// This can be used to find information for units that include a given header.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param path The source file to search for.
/// \param receiver A function to be called for each unit. The pointer is only valid for
/// the duration of the call. The function should return a true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_units_containing_file(
  _Nonnull indexstoredb_index_t index,
  const char *_Nonnull path,
  _Nonnull indexstoredb_unit_info_receiver receiver);

/// Return the file path which included by a given file path.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param path The source file to search for.
/// \param receiver A function to be called for each include file path. The pointers are only valid for
/// the duration of the call. The function should return a true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_files_included_by_file(
  _Nonnull indexstoredb_index_t index,
  const char *_Nonnull path,
  _Nonnull indexstoredb_file_includes_receiver receiver);

/// Return the file path which including a given header.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param path The source file to search for.
/// \param receiver A function to be called for each include file path. The pointers are only valid for
/// the duration of the call. The function should return a true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_files_including_file(
  _Nonnull indexstoredb_index_t index,
  const char *_Nonnull path,
  _Nonnull indexstoredb_file_includes_receiver receiver);

/// Iterates over recorded `#include`s of a unit.
///
/// \param index An IndexStoreDB object which contains the symbols.
/// \param unitName The unit name to search for.
/// \param receiver A function to be called for each include entry. The pointers are only valid for
/// the duration of the call. The function should return a true to continue iterating.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_includes_of_unit(
  _Nonnull indexstoredb_index_t index,
  const char *_Nonnull unitName,
  _Nonnull indexstoredb_unit_includes_receiver receiver);

INDEXSTOREDB_END_DECLS

#endif
