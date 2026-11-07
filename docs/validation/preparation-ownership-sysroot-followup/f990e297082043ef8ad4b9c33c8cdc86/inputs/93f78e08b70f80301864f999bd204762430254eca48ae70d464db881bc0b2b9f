# Script mode starts from the actual invoking process cwd, independently of $PWD.
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "Dependency resolution requires INPUT and OUTPUT")
endif()
file(REAL_PATH "${INPUT}" normalized BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
file(WRITE "${OUTPUT}" "${normalized}")
