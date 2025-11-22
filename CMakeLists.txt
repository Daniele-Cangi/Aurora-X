cmake_minimum_required(VERSION 3.20)
project(AuroraX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_FIELD          "Enable FIELD_BUILD (hardware/real) definitions" OFF)
option(USE_SODIUM           "Link against libsodium (enable real Ed25519)" ON)
option(USE_RAPTORQ          "Enable RaptorQ real implementation flags" ON)
option(RAPTORQ_HEADER_ONLY  "Use header-only libRaptorQ (RaptorQ_v1_hdr.hpp)" ON)
option(BUILD_NET_TOOLS      "Build Internet/batch demo utilities" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)

# -------------------------------------------------------------------
# Shared headers/interface target
add_library(aurora_headers INTERFACE)
target_include_directories(aurora_headers INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(BUILD_FIELD)
  target_compile_definitions(aurora_headers INTERFACE FIELD_BUILD)
endif()

# -------------------------------------------------------------------
# libsodium (strong crypto defaults)
set(AURORA_SODIUM_HINT_DIR "" CACHE PATH "Hint directory containing libsodium include/lib")
set(AURORA_SODIUM_ARCH "auto" CACHE STRING "sodium arch hint: auto|win64|win32")

if(USE_SODIUM)
  target_compile_definitions(aurora_headers INTERFACE AURORA_USE_REAL_CRYPTO PODM_USE_SODIUM)

  set(_SODIUM_CANDIDATE_DIRS
    ${AURORA_SODIUM_HINT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libsodium-win64
    ${CMAKE_CURRENT_SOURCE_DIR}/libsodium-win32
    ${CMAKE_CURRENT_SOURCE_DIR}/libsodium-1.0.18-mingw/libsodium-win64
    ${CMAKE_CURRENT_SOURCE_DIR}/libsodium-1.0.18-mingw/libsodium-win32
  )

  set(_SODIUM_INC_DIRS)
  set(_SODIUM_LIB_DIRS)
  foreach(dir ${_SODIUM_CANDIDATE_DIRS})
    if(EXISTS ${dir}/include)
      list(APPEND _SODIUM_INC_DIRS ${dir}/include)
    endif()
    if(EXISTS ${dir}/lib)
      list(APPEND _SODIUM_LIB_DIRS ${dir}/lib)
    endif()
  endforeach()
  if(_SODIUM_INC_DIRS)
    list(REMOVE_DUPLICATES _SODIUM_INC_DIRS)
  endif()
  if(_SODIUM_LIB_DIRS)
    list(REMOVE_DUPLICATES _SODIUM_LIB_DIRS)
  endif()

  find_library(SODIUM_LIB NAMES sodium libsodium PATHS ${_SODIUM_LIB_DIRS} NO_DEFAULT_PATH)
  if(NOT SODIUM_LIB)
    find_library(SODIUM_LIB NAMES sodium libsodium)
  endif()

  add_library(aurora_sodium INTERFACE)
  if(SODIUM_LIB)
    target_link_libraries(aurora_sodium INTERFACE ${SODIUM_LIB})
  endif()
  if(_SODIUM_INC_DIRS)
    target_include_directories(aurora_sodium INTERFACE ${_SODIUM_INC_DIRS})
  endif()

  if(NOT SODIUM_LIB)
    message(FATAL_ERROR "libsodium not found. Set USE_SODIUM=OFF to build without real crypto (NOT recommended) or provide AURORA_SODIUM_HINT_DIR.")
  endif()
endif()

# -------------------------------------------------------------------
# RaptorQ (optional but encouraged)
set(RAPTORQ_ROOT "" CACHE PATH "Root of RaptorQ SDK (include/lib)")
if(USE_RAPTORQ AND (NOT RAPTORQ_ROOT))
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/libRaptorQ)
    set(RAPTORQ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/extern/libRaptorQ CACHE PATH "Root of RaptorQ SDK (include/lib)" FORCE)
  endif()
endif()

set(AURORA_FEC_SOURCES)
if(USE_RAPTORQ)
  target_compile_definitions(aurora_headers INTERFACE AURORA_USE_LIBRAPTORQ=1 AURORA_USE_RAPTORQ_REAL)
  add_library(aurora_raptorq INTERFACE)
  if(RAPTORQ_HEADER_ONLY)
    if(RAPTORQ_ROOT)
      target_include_directories(aurora_raptorq INTERFACE 
        ${RAPTORQ_ROOT}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/eigen
      )
    endif()
  else()
    if(RAPTORQ_ROOT)
      target_include_directories(aurora_raptorq INTERFACE 
        ${RAPTORQ_ROOT}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/eigen
      )
      find_library(RAPTORQ_LIB NAMES RaptorQ raptorq PATHS ${RAPTORQ_ROOT}/lib)
    else()
      find_library(RAPTORQ_LIB NAMES RaptorQ raptorq)
    endif()
    if(NOT RAPTORQ_LIB)
      message(FATAL_ERROR "libRaptorQ not found. Set RAPTORQ_ROOT or enable RAPTORQ_HEADER_ONLY.")
    endif()
    target_link_libraries(aurora_raptorq INTERFACE ${RAPTORQ_LIB})
  endif()
  list(APPEND AURORA_FEC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/fec/AuroraRaptorQ.cpp)
endif()

# Helper macro for linking common deps
function(aurora_link_common target)
  target_link_libraries(${target} PRIVATE aurora_headers Threads::Threads)
  if(TARGET aurora_sodium)
    target_link_libraries(${target} PRIVATE aurora_sodium)
  endif()
  if(TARGET aurora_raptorq)
    target_link_libraries(${target} PRIVATE aurora_raptorq)
  endif()
endfunction()

# -------------------------------------------------------------------
# Primary targets
add_executable(aurora_x aurora_x.cpp ${AURORA_FEC_SOURCES})
aurora_link_common(aurora_x)

add_executable(test_podm test_podm.cpp)
aurora_link_common(test_podm)

if(USE_RAPTORQ)
  add_executable(test_raptorq_adapter tests/test_raptorq_adapter.cpp ${AURORA_FEC_SOURCES})
  aurora_link_common(test_raptorq_adapter)
endif()

# Optional Internet/batch tooling
if(BUILD_NET_TOOLS)
  set(AURORA_NET_TOOLS
    aurora_batch_test
    aurora_deadline_sweep
    aurora_selector_test
    aurora_udp_demo
    aurora_udp_extreme_bidir_test
    aurora_udp_extreme_echo_test
    aurora_udp_extreme_seq_test
    aurora_udp_extreme_test
    aurora_udp_token_seq_test
    aurora_unit_dedup_test
  )
  foreach(tool ${AURORA_NET_TOOLS})
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${tool}.cpp")
    if(EXISTS ${src})
      add_executable(${tool} ${src} ${AURORA_FEC_SOURCES})
      aurora_link_common(${tool})
      if(WIN32)
        target_link_libraries(${tool} PRIVATE ws2_32)
      endif()
    endif()
  endforeach()

  # Special definitions for specific tools
  if(TARGET aurora_unit_dedup_test)
    target_compile_definitions(aurora_unit_dedup_test PRIVATE AURORA_NO_MAIN)
  endif()
  if(TARGET aurora_batch_test)
    target_compile_definitions(aurora_batch_test PRIVATE AURORA_NO_MAIN)
  endif()
  if(TARGET aurora_deadline_sweep)
    target_compile_definitions(aurora_deadline_sweep PRIVATE AURORA_NO_MAIN)
  endif()
  if(TARGET aurora_selector_test)
    target_compile_definitions(aurora_selector_test PRIVATE AURORA_NO_MAIN)
  endif()
endif()

# -------------------------------------------------------------------
# Warning levels
set(_AURORA_ALL_TARGETS
  aurora_x
  test_podm
  test_raptorq_adapter
  aurora_batch_test
  aurora_deadline_sweep
  aurora_selector_test
  aurora_udp_demo
  aurora_udp_extreme_bidir_test
  aurora_udp_extreme_echo_test
  aurora_udp_extreme_seq_test
  aurora_udp_extreme_test
  aurora_udp_token_seq_test
  aurora_unit_dedup_test
)

foreach(tgt ${_AURORA_ALL_TARGETS})
  if(TARGET ${tgt})
    if(MSVC)
      target_compile_options(${tgt} PRIVATE /W4)
    else()
      target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
endforeach()

message(STATUS "")
message(STATUS "==== Aurora-X Build Configuration ====")
message(STATUS "  BUILD_FIELD     : ${BUILD_FIELD}")
message(STATUS "  USE_SODIUM      : ${USE_SODIUM}")
message(STATUS "  USE_RAPTORQ     : ${USE_RAPTORQ}")
message(STATUS "  RAPTORQ_HEADER  : ${RAPTORQ_HEADER_ONLY}")
message(STATUS "  BUILD_NET_TOOLS : ${BUILD_NET_TOOLS}")
message(STATUS "  Output Dir      : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "======================================")
