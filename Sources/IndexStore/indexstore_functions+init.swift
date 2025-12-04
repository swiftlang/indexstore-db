//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation
import IndexStoreDB_CIndexStoreDB

extension indexstore_functions_t {
  /// Load all Index Store functions from a dynamic library that was opened using `dlopen`. The `dlhandle` is the handle
  /// to the dynamic library that was returned from `dlopen`.
  init(dlHandle: DLHandle) throws {
    enum Error: Swift.Error {
      /// Loading a required symbol from the sourcekitd library failed.
      case missingRequiredSymbol(symbolName: String)
    }

    func loadRequired<T>(_ symbol: String) throws -> T {
      let symbol = "indexstore_\(symbol)"
      guard let sym: T = dlsym(dlHandle, symbol: symbol) else {
        throw Error.missingRequiredSymbol(symbolName: symbol)
      }
      return sym
    }

    func loadOptional<T>(_ symbol: String) -> T? {
      return dlsym(dlHandle, symbol: symbol)
    }

    // The symbols that take blocks are only available on Darwin (INDEXSTORE_HAS_BLOCKS). Unfortunately, there's no
    // better way to cover these than to duplicate the initializer
    #if canImport(Darwin)
    self.init(
      error_get_description: try loadRequired("error_get_description"),
      error_dispose: try loadRequired("error_dispose"),
      format_version: try loadRequired("format_version"),
      version: try loadRequired("version"),
      creation_options_create: try loadRequired("creation_options_create"),
      creation_options_dispose: try loadRequired("creation_options_dispose"),
      creation_options_add_prefix_mapping: try loadRequired("creation_options_add_prefix_mapping"),
      store_create: try loadRequired("store_create"),
      store_create_with_options: try loadRequired("store_create_with_options"),
      store_dispose: try loadRequired("store_dispose"),
      store_units_apply: try loadRequired("store_units_apply"),
      store_units_apply_f: try loadRequired("store_units_apply_f"),
      unit_event_notification_get_events_count: try loadRequired("unit_event_notification_get_events_count"),
      unit_event_notification_get_event: try loadRequired("unit_event_notification_get_event"),
      unit_event_notification_is_initial: try loadRequired("unit_event_notification_is_initial"),
      unit_event_get_kind: try loadRequired("unit_event_get_kind"),
      unit_event_get_unit_name: try loadRequired("unit_event_get_unit_name"),
      store_set_unit_event_handler: try loadRequired("store_set_unit_event_handler"),
      store_set_unit_event_handler_f: try loadRequired("store_set_unit_event_handler_f"),
      store_start_unit_event_listening: try loadRequired("store_start_unit_event_listening"),
      store_stop_unit_event_listening: try loadRequired("store_stop_unit_event_listening"),
      store_discard_unit: try loadRequired("store_discard_unit"),
      store_discard_record: try loadRequired("store_discard_record"),
      store_purge_stale_data: try loadRequired("store_purge_stale_data"),
      store_get_unit_name_from_output_path: try loadRequired("store_get_unit_name_from_output_path"),
      store_get_unit_modification_time: try loadRequired("store_get_unit_modification_time"),
      symbol_get_language: try loadRequired("symbol_get_language"),
      symbol_get_kind: try loadRequired("symbol_get_kind"),
      symbol_get_subkind: try loadRequired("symbol_get_subkind"),
      symbol_get_properties: try loadRequired("symbol_get_properties"),
      symbol_get_roles: try loadRequired("symbol_get_roles"),
      symbol_get_related_roles: try loadRequired("symbol_get_related_roles"),
      symbol_get_name: try loadRequired("symbol_get_name"),
      symbol_get_usr: try loadRequired("symbol_get_usr"),
      symbol_get_codegen_name: try loadRequired("symbol_get_codegen_name"),
      symbol_relation_get_roles: try loadRequired("symbol_relation_get_roles"),
      symbol_relation_get_symbol: try loadRequired("symbol_relation_get_symbol"),
      occurrence_get_symbol: try loadRequired("occurrence_get_symbol"),
      occurrence_relations_apply: try loadRequired("occurrence_relations_apply"),
      occurrence_relations_apply_f: try loadRequired("occurrence_relations_apply_f"),
      occurrence_get_roles: try loadRequired("occurrence_get_roles"),
      occurrence_get_line_col: try loadRequired("occurrence_get_line_col"),
      record_reader_create: try loadRequired("record_reader_create"),
      record_reader_dispose: try loadRequired("record_reader_dispose"),
      record_reader_search_symbols: try loadRequired("record_reader_search_symbols"),
      record_reader_symbols_apply: try loadRequired("record_reader_symbols_apply"),
      record_reader_occurrences_apply: try loadRequired("record_reader_occurrences_apply"),
      record_reader_occurrences_in_line_range_apply: try loadRequired("record_reader_occurrences_in_line_range_apply"),
      record_reader_occurrences_of_symbols_apply: try loadRequired("record_reader_occurrences_of_symbols_apply"),
      record_reader_search_symbols_f: try loadRequired("record_reader_search_symbols_f"),
      record_reader_symbols_apply_f: try loadRequired("record_reader_symbols_apply_f"),
      record_reader_occurrences_apply_f: try loadRequired("record_reader_occurrences_apply_f"),
      record_reader_occurrences_in_line_range_apply_f: try loadRequired(
        "record_reader_occurrences_in_line_range_apply_f"
      ),
      record_reader_occurrences_of_symbols_apply_f: try loadRequired("record_reader_occurrences_of_symbols_apply_f"),
      unit_reader_create: try loadRequired("unit_reader_create"),
      unit_reader_dispose: try loadRequired("unit_reader_dispose"),
      unit_reader_get_provider_identifier: try loadRequired("unit_reader_get_provider_identifier"),
      unit_reader_get_provider_version: try loadRequired("unit_reader_get_provider_version"),
      unit_reader_get_modification_time: try loadRequired("unit_reader_get_modification_time"),
      unit_reader_is_system_unit: try loadRequired("unit_reader_is_system_unit"),
      unit_reader_is_module_unit: try loadRequired("unit_reader_is_module_unit"),
      unit_reader_is_debug_compilation: try loadRequired("unit_reader_is_debug_compilation"),
      unit_reader_has_main_file: try loadRequired("unit_reader_has_main_file"),
      unit_reader_get_main_file: try loadRequired("unit_reader_get_main_file"),
      unit_reader_get_module_name: try loadRequired("unit_reader_get_module_name"),
      unit_reader_get_working_dir: try loadRequired("unit_reader_get_working_dir"),
      unit_reader_get_output_file: try loadRequired("unit_reader_get_output_file"),
      unit_reader_get_sysroot_path: try loadRequired("unit_reader_get_sysroot_path"),
      unit_reader_get_target: try loadRequired("unit_reader_get_target"),
      unit_dependency_get_kind: try loadRequired("unit_dependency_get_kind"),
      unit_dependency_is_system: try loadRequired("unit_dependency_is_system"),
      unit_dependency_get_filepath: try loadRequired("unit_dependency_get_filepath"),
      unit_dependency_get_modulename: try loadRequired("unit_dependency_get_modulename"),
      unit_dependency_get_name: try loadRequired("unit_dependency_get_name"),
      unit_include_get_source_path: try loadRequired("unit_include_get_source_path"),
      unit_include_get_target_path: try loadRequired("unit_include_get_target_path"),
      unit_include_get_source_line: try loadRequired("unit_include_get_source_line"),
      unit_reader_dependencies_apply: try loadRequired("unit_reader_dependencies_apply"),
      unit_reader_includes_apply: try loadRequired("unit_reader_includes_apply"),
      unit_reader_dependencies_apply_f: try loadRequired("unit_reader_dependencies_apply_f"),
      unit_reader_includes_apply_f: try loadRequired("unit_reader_includes_apply_f"),
    )
    #else
    self.init(
      error_get_description: try loadRequired("error_get_description"),
      error_dispose: try loadRequired("error_dispose"),
      format_version: try loadRequired("format_version"),
      version: try loadRequired("version"),
      creation_options_create: try loadRequired("creation_options_create"),
      creation_options_dispose: try loadRequired("creation_options_dispose"),
      creation_options_add_prefix_mapping: try loadRequired("creation_options_add_prefix_mapping"),
      store_create: try loadRequired("store_create"),
      store_create_with_options: try loadRequired("store_create_with_options"),
      store_dispose: try loadRequired("store_dispose"),
      store_units_apply_f: try loadRequired("store_units_apply_f"),
      unit_event_notification_get_events_count: try loadRequired("unit_event_notification_get_events_count"),
      unit_event_notification_get_event: try loadRequired("unit_event_notification_get_event"),
      unit_event_notification_is_initial: try loadRequired("unit_event_notification_is_initial"),
      unit_event_get_kind: try loadRequired("unit_event_get_kind"),
      unit_event_get_unit_name: try loadRequired("unit_event_get_unit_name"),
      store_set_unit_event_handler_f: try loadRequired("store_set_unit_event_handler_f"),
      store_start_unit_event_listening: try loadRequired("store_start_unit_event_listening"),
      store_stop_unit_event_listening: try loadRequired("store_stop_unit_event_listening"),
      store_discard_unit: try loadRequired("store_discard_unit"),
      store_discard_record: try loadRequired("store_discard_record"),
      store_purge_stale_data: try loadRequired("store_purge_stale_data"),
      store_get_unit_name_from_output_path: try loadRequired("store_get_unit_name_from_output_path"),
      store_get_unit_modification_time: try loadRequired("store_get_unit_modification_time"),
      symbol_get_language: try loadRequired("symbol_get_language"),
      symbol_get_kind: try loadRequired("symbol_get_kind"),
      symbol_get_subkind: try loadRequired("symbol_get_subkind"),
      symbol_get_properties: try loadRequired("symbol_get_properties"),
      symbol_get_roles: try loadRequired("symbol_get_roles"),
      symbol_get_related_roles: try loadRequired("symbol_get_related_roles"),
      symbol_get_name: try loadRequired("symbol_get_name"),
      symbol_get_usr: try loadRequired("symbol_get_usr"),
      symbol_get_codegen_name: try loadRequired("symbol_get_codegen_name"),
      symbol_relation_get_roles: try loadRequired("symbol_relation_get_roles"),
      symbol_relation_get_symbol: try loadRequired("symbol_relation_get_symbol"),
      occurrence_get_symbol: try loadRequired("occurrence_get_symbol"),
      occurrence_relations_apply_f: try loadRequired("occurrence_relations_apply_f"),
      occurrence_get_roles: try loadRequired("occurrence_get_roles"),
      occurrence_get_line_col: try loadRequired("occurrence_get_line_col"),
      record_reader_create: try loadRequired("record_reader_create"),
      record_reader_dispose: try loadRequired("record_reader_dispose"),
      record_reader_search_symbols_f: try loadRequired("record_reader_search_symbols_f"),
      record_reader_symbols_apply_f: try loadRequired("record_reader_symbols_apply_f"),
      record_reader_occurrences_apply_f: try loadRequired("record_reader_occurrences_apply_f"),
      record_reader_occurrences_in_line_range_apply_f: try loadRequired(
        "record_reader_occurrences_in_line_range_apply_f"
      ),
      record_reader_occurrences_of_symbols_apply_f: try loadRequired("record_reader_occurrences_of_symbols_apply_f"),
      unit_reader_create: try loadRequired("unit_reader_create"),
      unit_reader_dispose: try loadRequired("unit_reader_dispose"),
      unit_reader_get_provider_identifier: try loadRequired("unit_reader_get_provider_identifier"),
      unit_reader_get_provider_version: try loadRequired("unit_reader_get_provider_version"),
      unit_reader_get_modification_time: try loadRequired("unit_reader_get_modification_time"),
      unit_reader_is_system_unit: try loadRequired("unit_reader_is_system_unit"),
      unit_reader_is_module_unit: try loadRequired("unit_reader_is_module_unit"),
      unit_reader_is_debug_compilation: try loadRequired("unit_reader_is_debug_compilation"),
      unit_reader_has_main_file: try loadRequired("unit_reader_has_main_file"),
      unit_reader_get_main_file: try loadRequired("unit_reader_get_main_file"),
      unit_reader_get_module_name: try loadRequired("unit_reader_get_module_name"),
      unit_reader_get_working_dir: try loadRequired("unit_reader_get_working_dir"),
      unit_reader_get_output_file: try loadRequired("unit_reader_get_output_file"),
      unit_reader_get_sysroot_path: try loadRequired("unit_reader_get_sysroot_path"),
      unit_reader_get_target: try loadRequired("unit_reader_get_target"),
      unit_dependency_get_kind: try loadRequired("unit_dependency_get_kind"),
      unit_dependency_is_system: try loadRequired("unit_dependency_is_system"),
      unit_dependency_get_filepath: try loadRequired("unit_dependency_get_filepath"),
      unit_dependency_get_modulename: try loadRequired("unit_dependency_get_modulename"),
      unit_dependency_get_name: try loadRequired("unit_dependency_get_name"),
      unit_include_get_source_path: try loadRequired("unit_include_get_source_path"),
      unit_include_get_target_path: try loadRequired("unit_include_get_target_path"),
      unit_include_get_source_line: try loadRequired("unit_include_get_source_line"),
      unit_reader_dependencies_apply_f: try loadRequired("unit_reader_dependencies_apply_f"),
      unit_reader_includes_apply_f: try loadRequired("unit_reader_includes_apply_f"),
    )
    #endif
  }
}
