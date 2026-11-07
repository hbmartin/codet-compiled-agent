# Cache values are provisional until generation publishes their matching receipt.
include("${CMAKE_CURRENT_LIST_DIR}/json.cmake")
set(_codet_dependency_pin "${CMAKE_BINARY_DIR}/CMakeFiles/codet-dependency-selection.json")
set(_codet_dependency_origin "")
set(_codet_dependency_token "")
get_property(_codet_has_cache CACHE CODET_DEPENDENCY_ROOT PROPERTY TYPE SET)
set(_codet_cache_value "$CACHE{CODET_DEPENDENCY_ROOT}")
set(_codet_has_normal FALSE)
if(DEFINED CODET_DEPENDENCY_ROOT AND NOT CODET_DEPENDENCY_ROOT STREQUAL "" AND
   (NOT _codet_has_cache OR NOT CODET_DEPENDENCY_ROOT STREQUAL _codet_cache_value))
  set(_codet_has_normal TRUE)
  set(_codet_normal_value "${CODET_DEPENDENCY_ROOT}")
endif()
if(NOT _codet_has_cache OR _codet_cache_value STREQUAL "")
  # Both -U and an explicit empty override reset discovery, including obsolete pins.
  file(REMOVE "${_codet_dependency_pin}")
  unset(_CODET_DEPENDENCY_VALUE CACHE)
  unset(_CODET_DEPENDENCY_ORIGIN CACHE)
  unset(_CODET_DEPENDENCY_TOKEN CACHE)
endif()
if(_codet_has_cache AND NOT _codet_cache_value STREQUAL "" AND
   (NOT DEFINED _CODET_DEPENDENCY_VALUE OR
    NOT _codet_cache_value STREQUAL _CODET_DEPENDENCY_VALUE OR
    _CODET_DEPENDENCY_ORIGIN STREQUAL "explicit"))
  set(_codet_dependency_value "${_codet_cache_value}")
  set(_codet_dependency_origin "explicit")
elseif(_codet_has_normal)
  # A normal variable is scoped input, not a persistent cache override.
  set(_codet_dependency_value "${_codet_normal_value}")
  set(_codet_dependency_origin "normal")
elseif(EXISTS "${_codet_dependency_pin}")
  file(READ "${_codet_dependency_pin}" _codet_selection)
  foreach(_field format root origin source token)
    string(JSON _kind ERROR_VARIABLE _error TYPE "${_codet_selection}" ${_field})
    if(NOT _error STREQUAL "NOTFOUND" OR
       (_field STREQUAL "format" AND NOT _kind STREQUAL "NUMBER") OR
       (NOT _field STREQUAL "format" AND NOT _kind STREQUAL "STRING"))
      message(FATAL_ERROR "Invalid dependency selection record ${_codet_dependency_pin}; reset with -UCODET_DEPENDENCY_ROOT or recreate the build directory")
    endif()
    string(JSON _codet_selection_${_field} GET "${_codet_selection}" ${_field})
  endforeach()
  if(NOT _codet_selection_format EQUAL 1 OR
     NOT _codet_selection_source STREQUAL CMAKE_SOURCE_DIR OR
     NOT _codet_selection_origin MATCHES "^(environment|default)$" OR
     NOT IS_ABSOLUTE "${_codet_selection_root}" OR
     NOT _codet_selection_root STREQUAL _CODET_DEPENDENCY_VALUE OR
     NOT _codet_selection_token STREQUAL _CODET_DEPENDENCY_TOKEN OR
     NOT _codet_selection_origin STREQUAL _CODET_DEPENDENCY_ORIGIN)
    message(FATAL_ERROR "Invalid or obsolete dependency selection record ${_codet_dependency_pin}; reset with -UCODET_DEPENDENCY_ROOT or recreate the build directory")
  endif()
  set(_codet_dependency_value "${_codet_selection_root}")
  set(_codet_dependency_origin "${_codet_selection_origin}")
  set(_codet_dependency_token "${_codet_selection_token}")
endif()
if(_codet_dependency_origin STREQUAL "")
  # A failed first configure may leave provisional cache metadata, never an override.
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
file(READ "${_codet_dependency_result}" _codet_dependency_value)
if(NOT _codet_dependency_origin STREQUAL "normal")
  set(CODET_DEPENDENCY_ROOT "${_codet_dependency_value}" CACHE PATH "Offline dependency sources; empty or -U resets selection" FORCE)
  set(_CODET_DEPENDENCY_VALUE "${_codet_dependency_value}" CACHE INTERNAL "Provisional dependency selection" FORCE)
  set(_CODET_DEPENDENCY_ORIGIN "${_codet_dependency_origin}" CACHE INTERNAL "Dependency selection origin" FORCE)
  if(_codet_dependency_origin STREQUAL "explicit")
    file(REMOVE "${_codet_dependency_pin}")
    unset(_CODET_DEPENDENCY_TOKEN CACHE)
  else()
    if(_codet_dependency_token STREQUAL "")
      string(RANDOM LENGTH 32 ALPHABET 0123456789abcdef _codet_dependency_token)
    endif()
    set(_CODET_DEPENDENCY_TOKEN "${_codet_dependency_token}" CACHE INTERNAL "Generation receipt identity" FORCE)
    _codet_json_string(_root "${_codet_dependency_value}")
    _codet_json_string(_origin "${_codet_dependency_origin}")
    _codet_json_string(_source "${CMAKE_SOURCE_DIR}")
    _codet_json_string(_token "${_codet_dependency_token}")
    # SEND_ERROR prevents generation; later generator errors may retain this receipt.
    file(GENERATE OUTPUT "${_codet_dependency_pin}"
      CONTENT "{\"format\":1,\"root\":${_root},\"origin\":${_origin},\"source\":${_source},\"token\":${_token}}\n")
  endif()
endif()
set(CODET_DEPENDENCY_ROOT "${_codet_dependency_value}")
