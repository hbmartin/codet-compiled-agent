# An empty cache entry is a resettable marker, not the selected path. The
# selection record pins derived choices when project configuration completes.
set(_codet_dependency_pin "${CMAKE_BINARY_DIR}/CMakeFiles/codet-dependency-selection.json")
set(_codet_dependency_origin "explicit")
if(DEFINED CODET_DEPENDENCY_ROOT AND NOT CODET_DEPENDENCY_ROOT STREQUAL "")
  set(_codet_dependency_value "${CODET_DEPENDENCY_ROOT}")
elseif(DEFINED CACHE{CODET_DEPENDENCY_ROOT} AND EXISTS "${_codet_dependency_pin}")
  file(READ "${_codet_dependency_pin}" _codet_selection)
  string(JSON _codet_dependency_value GET "${_codet_selection}" root)
  string(JSON _codet_dependency_origin GET "${_codet_selection}" origin)
  string(JSON _codet_selection_source GET "${_codet_selection}" source)
  if(NOT _codet_selection_source STREQUAL CMAKE_SOURCE_DIR OR
     NOT _codet_dependency_origin MATCHES "^(environment|default)$" OR
     NOT IS_ABSOLUTE "${_codet_dependency_value}")
    message(FATAL_ERROR "Invalid dependency selection record; reset with -UCODET_DEPENDENCY_ROOT")
  endif()
else()
  # Absence of the public cache marker is an explicit reset, including after -U.
  file(REMOVE "${_codet_dependency_pin}")
  if(NOT "$ENV{CODET_DEPENDENCY_ROOT}" STREQUAL "")
    set(_codet_dependency_value "$ENV{CODET_DEPENDENCY_ROOT}")
    set(_codet_dependency_origin "environment")
  else()
    set(_codet_dependency_value "${CMAKE_SOURCE_DIR}/.deps")
    set(_codet_dependency_origin "default")
  endif()
endif()
set(_codet_dependency_result "${CMAKE_BINARY_DIR}/CMakeFiles/codet-dependency-root.txt")
file(REMOVE "${_codet_dependency_result}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DINPUT:STRING=${_codet_dependency_value}"
  "-DOUTPUT:FILEPATH=${_codet_dependency_result}" -P "${CMAKE_CURRENT_LIST_DIR}/resolve-dependency-root.cmake"
  RESULT_VARIABLE _codet_dependency_status ERROR_VARIABLE _codet_dependency_error)
if(NOT _codet_dependency_status EQUAL 0)
  message(FATAL_ERROR "Cannot resolve dependency root: ${_codet_dependency_error}")
endif()
file(READ "${_codet_dependency_result}" CODET_DEPENDENCY_ROOT)
function(_codet_commit_dependency_selection)
  if(_codet_dependency_origin STREQUAL "explicit")
    set(CODET_DEPENDENCY_ROOT "${CODET_DEPENDENCY_ROOT}" CACHE PATH "Offline dependency sources" FORCE)
  else()
    set(CODET_DEPENDENCY_ROOT "" CACHE PATH "Dependency override; -U resets the generated selection" FORCE)
  endif()
  # Commit after the project finishes configuring. Later generator errors do
  # not change a validated selection; -U explicitly resets it.
  _codet_json_string(_root "${_codet_dependency_value}")
  _codet_json_string(_origin "${_codet_dependency_origin}")
  _codet_json_string(_source "${CMAKE_SOURCE_DIR}")
  file(WRITE "${_codet_dependency_pin}.tmp"
    "{\"root\":${_root},\"origin\":${_origin},\"source\":${_source}}\n")
  file(RENAME "${_codet_dependency_pin}.tmp" "${_codet_dependency_pin}")
endfunction()
# Persist the resolved spelling, not the relative input.
set(_codet_dependency_value "${CODET_DEPENDENCY_ROOT}")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _codet_commit_dependency_selection)
