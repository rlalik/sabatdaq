cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    sabatdaq
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Dependencies ----

find_path(JANUS_PATH Janus_Install.bash HINT ${JANUS_DIR})
if (JANUS_PATH)
message(STATUS "Found JanusC in ${JANUS_PATH}")
else()
message(FATAL_ERROR "JanusC not found")
endif()
# option(JANUS_PATH )

add_library(JanusC_JanusC INTERFACE IMPORTED)
add_library(JanusC::JanusC ALIAS JanusC_JanusC)
target_include_directories(JanusC_JanusC
    INTERFACE ${JANUS_PATH}/src
)

add_library(FERS_FERS INTERFACE IMPORTED)
add_library(FERS::FERS ALIAS FERS_FERS)
target_include_directories(FERS_FERS
    INTERFACE ${JANUS_PATH}/ferslib/include
)
target_link_directories(FERS_FERS
    INTERFACE ${JANUS_PATH}/ferslib/local/lib/
)
target_link_libraries(FERS_FERS
    INTERFACE caenferslib
)

# ---- Declare library ----

add_library(
    sabatdaq_lib OBJECT
    source/lib.cpp
)

target_include_directories(
    sabatdaq_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(sabatdaq_lib PUBLIC cxx_std_23)

target_link_libraries(sabatdaq_lib PUBLIC JanusC::JanusC FERS::FERS)

# ---- Declare executable ----

configure_file(config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

add_executable(sabatdaq_exe source/main.cpp)
add_executable(sabatdaq::exe ALIAS sabatdaq_exe)

set_property(TARGET sabatdaq_exe PROPERTY OUTPUT_NAME sabatdaq)

target_compile_features(sabatdaq_exe PRIVATE cxx_std_20)
target_include_directories(sabatdaq_exe PRIVATE ${CMAKE_BINARY_DIR}/include)
target_link_libraries(sabatdaq_exe PRIVATE sabatdaq_lib)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT sabatdaq_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of sabatdaq"
  )
endif()

include(cmake/dev-mode.cmake)
