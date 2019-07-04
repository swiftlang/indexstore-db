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
# if defined (_MSC_VER)
#  define INDEXSTOREDB_PUBLIC __declspec(dllimport)
# else
#  define INDEXSTOREDB_PUBLIC
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
typedef indexstoredb_object_t indexstoredb_symbol_t;
typedef indexstoredb_object_t indexstoredb_symbol_occurrence_t;

typedef void *indexstoredb_error_t;
typedef void *indexstoredb_symbol_location_t;

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

/// Returns true on success.
typedef _Nullable indexstoredb_indexstore_library_t(^indexstore_library_provider_t)(const char * _Nonnull);

/// Returns true to continue.
typedef bool(^indexstoredb_symbol_occurrence_receiver_t)(_Nonnull indexstoredb_symbol_occurrence_t);

/// Returns true to continue.
typedef bool(^indexstoredb_symbol_name_receiver)(const char *_Nonnull);

INDEXSTOREDB_PUBLIC _Nullable
indexstoredb_index_t
indexstoredb_index_create(const char * _Nonnull storePath,
                  const char * _Nonnull databasePath,
                  _Nonnull indexstore_library_provider_t libProvider,
                  // delegate,
                  bool readonly,
                  indexstoredb_error_t _Nullable * _Nullable);

INDEXSTOREDB_PUBLIC _Nullable
indexstoredb_indexstore_library_t
indexstoredb_load_indexstore_library(const char * _Nonnull dylibPath,
                             indexstoredb_error_t _Nullable * _Nullable);

INDEXSTOREDB_PUBLIC bool
indexstoredb_index_symbol_occurrences_by_usr(
    _Nonnull indexstoredb_index_t index,
    const char *_Nonnull usr,
    uint64_t roles,
    _Nonnull indexstoredb_symbol_occurrence_receiver_t);

INDEXSTOREDB_PUBLIC bool
indexstoredb_index_related_symbol_occurrences_by_usr(
    _Nonnull indexstoredb_index_t index,
    const char *_Nonnull usr,
    uint64_t roles,
    _Nonnull indexstoredb_symbol_occurrence_receiver_t);

INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_usr(_Nonnull indexstoredb_symbol_t);

INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_name(_Nonnull indexstoredb_symbol_t);

INDEXSTOREDB_PUBLIC
_Nonnull indexstoredb_symbol_t
indexstoredb_symbol_occurrence_symbol(_Nonnull indexstoredb_symbol_occurrence_t);

INDEXSTOREDB_PUBLIC uint64_t
indexstoredb_symbol_occurrence_roles(_Nonnull indexstoredb_symbol_occurrence_t);

/// The location is owned by the occurrence and shall not be used after the occurrence is freed.
INDEXSTOREDB_PUBLIC _Nonnull
indexstoredb_symbol_location_t
indexstoredb_symbol_occurrence_location(_Nonnull indexstoredb_symbol_occurrence_t);

INDEXSTOREDB_PUBLIC
const char * _Nonnull
indexstoredb_symbol_location_path(_Nonnull indexstoredb_symbol_location_t);

INDEXSTOREDB_PUBLIC bool
indexstoredb_symbol_location_is_system(_Nonnull indexstoredb_symbol_location_t);

INDEXSTOREDB_PUBLIC int
indexstoredb_symbol_location_line(_Nonnull indexstoredb_symbol_location_t);

INDEXSTOREDB_PUBLIC int
indexstoredb_symbol_location_column_utf8(_Nonnull indexstoredb_symbol_location_t);

INDEXSTOREDB_PUBLIC _Nonnull
indexstoredb_object_t
indexstoredb_retain(_Nonnull indexstoredb_object_t);

INDEXSTOREDB_PUBLIC void
indexstoredb_release(_Nonnull indexstoredb_object_t);

INDEXSTOREDB_PUBLIC const char * _Nonnull
indexstoredb_error_get_description(_Nonnull indexstoredb_error_t);

INDEXSTOREDB_PUBLIC void
indexstoredb_error_dispose(_Nullable indexstoredb_error_t);

/// Loops through each symbol in the index and calls the receiver function with each symbol.
/// @param index An IndexStoreDB object which contains the symbols.
/// @param receiver A function to be called for each symbol, the CString of the symbol will be passed in to this function.
/// The function should return a boolean indicating whether the looping should continue.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_symbol_names(_Nonnull indexstoredb_index_t index, _Nonnull indexstoredb_symbol_name_receiver);

/// Loops through each canonical symbol that matches the string and performs the passed in function.
/// @param index An IndexStoreDB object which contains the symbols.
/// @param symbolName The name of the symbol whose canonical occurence should be found.
/// @param receiver A function to be called for each canonical occurence.
/// The SymbolOccurenceRef of the symbol will be passed in to this function.
/// The function should return a boolean indicating whether the looping should continue.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_canonical_symbol_occurences_by_name(
    indexstoredb_index_t _Nonnull index,
    const char *_Nonnull symbolName,
    indexstoredb_symbol_occurrence_receiver_t _Nonnull receiver
);

/// Loops through each canonical symbol that matches the pattern and performs the passed in function.
/// @param index An IndexStoreDB object which contains the symbols.
/// @param anchorStart When true, symbol names should only be considered matching when the first characters of the symbol name match the pattern.
/// @param anchorEnd When true, symbol names should only be considered matching when the first characters of the symbol name match the pattern.
/// @param subsequence When true, symbols will be matched even if the pattern is not matched contiguously.
/// @param ignoreCase When true, symbols may be returned even if the case of letters does not match the pattern.
/// @param receiver A function to be called for each canonical occurence that matches the pattern.
/// The SymbolOccurenceRef of the symbol will be passed in to this function.
/// The function should return a boolean indicating whether the looping should continue.
INDEXSTOREDB_PUBLIC bool
indexstoredb_index_canonical_symbol_occurences_containing_pattern(
    _Nonnull indexstoredb_index_t index,
    const char *_Nonnull pattern,
    bool anchorStart,
    bool anchorEnd,
    bool subsequence,
    bool ignoreCase,
    _Nonnull indexstoredb_symbol_occurrence_receiver_t receiver);

INDEXSTOREDB_END_DECLS

#endif
