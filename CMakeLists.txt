cmake_minimum_required(VERSION 3.16)
project(cadical_propagator_template LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# CaDiCaL: lives in ./cadical and is built with its own configure/make flow.
# ---------------------------------------------------------------------------
set(CADICAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cadical)
set(CADICAL_LIB ${CADICAL_DIR}/build/libcadical.a)

if(NOT EXISTS ${CADICAL_DIR}/configure)
  message(FATAL_ERROR "CaDiCaL not found in ${CADICAL_DIR}. "
    "Clone it first:  git clone https://github.com/arminbiere/cadical.git cadical")
endif()

add_custom_command(
  OUTPUT ${CADICAL_LIB}
  COMMAND ${CMAKE_COMMAND} -E env CXX=${CMAKE_CXX_COMPILER} ./configure
  COMMAND make -j
  WORKING_DIRECTORY ${CADICAL_DIR}
  COMMENT "Building CaDiCaL (configure + make)"
  VERBATIM)

add_custom_target(cadical_build DEPENDS ${CADICAL_LIB})

add_library(cadical STATIC IMPORTED GLOBAL)
set_target_properties(cadical PROPERTIES
  IMPORTED_LOCATION ${CADICAL_LIB}
  INTERFACE_INCLUDE_DIRECTORIES ${CADICAL_DIR}/src)

# ---------------------------------------------------------------------------
# Template executable
# ---------------------------------------------------------------------------
add_executable(propagator_demo main.cpp)
add_dependencies(propagator_demo cadical_build)
target_link_libraries(propagator_demo PRIVATE cadical)
target_compile_options(propagator_demo PRIVATE -Wall -Wextra)
