cmake_minimum_required(VERSION 3.25)

project(asryx LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ccache: use if available on host
find_program(CCACHE_PATH ccache)
if(CCACHE_PATH)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PATH}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PATH}")
  message(STATUS "ccache found ===> enabling compiler cache")
endif()

include(Warnings)
include(Hardening)
include(Sanitizers)

# strict c++23 without gnu extensions
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" OFF)

set(
  BACKEND
  "cpu"
  CACHE STRING
  "Inference backend: cpu, cuda, or vulkan"
)

set_property(
  CACHE BACKEND
  PROPERTY STRINGS cpu cuda vulkan
)

set(WHISPER_SOURCE_DIR "$ENV{HOME}/.local/share/asryx/deps/whisper.cpp")

if(NOT EXISTS "${WHISPER_SOURCE_DIR}/CMakeLists.txt")
  message(
    FATAL_ERROR
      "missing whisper.cpp checkout: ${WHISPER_SOURCE_DIR}. run ./package/install first."
  )
endif()

set(SUPPORTED_BACKENDS cpu cuda vulkan)

if(NOT BACKEND IN_LIST SUPPORTED_BACKENDS)
  message(
    FATAL_ERROR
      "unsupported BACKEND='${BACKEND}'; expected cpu, cuda, or vulkan"
  )
endif()

# whisper.cpp & GGML overrides
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)

# silence 3rd-party compiler warnings
set(WHISPER_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(WHISPER_ALL_WARNINGS_3RD_PARTY OFF CACHE BOOL "" FORCE)

# strip out ancillary binaries & networking features
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_CURL OFF CACHE BOOL "" FORCE)

# disable upstream io integrations. This prevents the final binary from dynamically 
# linking against libsdl2 or libavcodec on host.
set(WHISPER_SDL2 OFF CACHE BOOL "" FORCE)
set(WHISPER_FFMPEG OFF CACHE BOOL "" FORCE)

# linux only => disable apple backends
set(GGML_METAL OFF CACHE BOOL "" FORCE)
# don't waste cycles probing for metal/coreml framework headers.
set(WHISPER_COREML OFF CACHE BOOL "" FORCE)

# disable RPC (all inference is strictly local)
set(GGML_RPC OFF CACHE BOOL "" FORCE)
# and disable BLAS (GGML's native cpu fallbacks handle AVX/NEON tensor math well enough)
set(GGML_BLAS OFF CACHE BOOL "" FORCE)

# OpenPM, use if installed, otherwise fall back to std::thread
find_package(OpenMP)
if(OpenMP_FOUND)
  set(GGML_OPENMP ON CACHE BOOL "" FORCE)
  message(STATUS "OpenMP found ===> enabling multi-threading support for GGML")
else()
  set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
  message(STATUS "OpenMP not found! GGML will fall back to std C++ threads")
endif()

if(BACKEND STREQUAL "cuda")
  set(GGML_CUDA ON CACHE BOOL "" FORCE)
  set(BACKEND_DEFINITION ASRYX_BACKEND_CUDA=1)
elseif(BACKEND STREQUAL "vulkan")
  set(GGML_VULKAN ON CACHE BOOL "" FORCE)
  set(BACKEND_DEFINITION ASRYX_BACKEND_VULKAN=1)
else()
  set(BACKEND_DEFINITION ASRYX_BACKEND_CPU=1)
endif()

message(STATUS "Backend: ${BACKEND}")

# embed whisper.cpp directly into the build tree
add_subdirectory("${WHISPER_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/whisper.cpp" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.hpp")

add_executable(asryx ${SOURCES} ${HEADERS})
target_include_directories(asryx PRIVATE "${PROJECT_SOURCE_DIR}/src")
# compile-time branching via #ifdef blocks.
target_compile_definitions(asryx PRIVATE ${BACKEND_DEFINITION})

target_link_libraries(asryx PRIVATE whisper)

if(OpenMP_FOUND)
  target_link_libraries(asryx PRIVATE OpenMP::OpenMP_CXX)
endif()

# exceptions? what? we don't do that here
target_compile_options(asryx PRIVATE -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
target_link_options(asryx PRIVATE -Wl,--gc-sections)

target_compile_options(asryx PRIVATE -march=native)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_property(TARGET asryx PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

  # mold linker, use if present for faster LTO link step
  find_program(MOLD_PATH mold)
  if(MOLD_PATH)
    message(STATUS "mold found ===> enabling ultra-fast LTO linking")
    target_link_options(asryx PRIVATE "-fuse-ld=mold")
  endif()

  # strip symbols and debug tables from the final executable
  add_custom_command(TARGET asryx POST_BUILD
    COMMAND "${CMAKE_STRIP}" -s $<TARGET_FILE:asryx>
  )
endif()

enable_warnings(asryx)
enable_hardening(asryx)
enable_sanitizers(asryx)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# LSP
add_custom_target(
  copy_compile_commands ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${CMAKE_BINARY_DIR}/compile_commands.json"
  "${CMAKE_SOURCE_DIR}/compile_commands.json"
  DEPENDS asryx
)