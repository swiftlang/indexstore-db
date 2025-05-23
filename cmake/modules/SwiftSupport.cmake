if(NOT IndexStoreDB_SWIFTMODULE_TRIPLE)
  set(target_info_cmd "${CMAKE_Swift_COMPILER}" -print-target-info)
  if(CMAKE_Swift_COMPILER_TARGET)
    list(APPEND target_info_cmd -target ${CMAKE_Swift_COMPILER_TARGET})
  endif()
  execute_process(COMMAND ${target_info_cmd} OUTPUT_VARIABLE target_info)
  string(JSON module_triple GET "${target_info}" "target" "moduleTriple")
  set(IndexStoreDB_SWIFTMODULE_TRIPLE "${module_triple}" CACHE STRING "Triple used to install swift module files")
  mark_as_advanced(IndexStoreDB_SWIFTMODULE_TRIPLE)
  message(CONFIGURE_LOG "Swift module triple: ${module_triple}")
endif()

function(install_swiftmodule target)
  set(swift_os $<LOWER_CASE:${CMAKE_SYSTEM_NAME}>)
  set(swift_dir $<IF:$<STREQUAL:$<TARGET_PROPERTY:${target},TYPE>,"STATIC_LIBRARY">,swift_static,swift>)
  get_target_property(module_name ${target} Swift_MODULE_NAME)
  if(NOT module_name)
    set(module_name ${target})
  endif()
  install(FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module_name}.swiftdoc
    DESTINATION "lib/${swift_dir}/${swift_os}/${module_name}.swiftmodule"
    RENAME ${IndexStoreDB_SWIFTMODULE_TRIPLE}.swiftdoc)
  install(FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module_name}.swiftmodule
    DESTINATION "lib/${swift_dir}/${swift_os}/${module_name}.swiftmodule"
    RENAME ${IndexStoreDB_SWIFTMODULE_TRIPLE}.swiftmodule)
endfunction()
