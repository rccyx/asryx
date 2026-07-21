cmake_minimum_required(VERSION 3.25)

project(asryx LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Warnings)
include(Hardening)
include(Sanitizers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ASRYX_BUILD_TESTS "Build Asryx tests" OFF)

set(
  ASRYX_BACKEND
  "cpu"
  CACHE STRING
  "Asryx inference backend: cpu, cuda, or vulkan"
)

set_property(
  CACHE ASRYX_BACKEND
  PROPERTY STRINGS cpu cuda vulkan
)

set(ASRYX_WHISPER_SOURCE_DIR "$ENV{HOME}/.local/opt/whisper.cpp")

if(NOT EXISTS "${ASRYX_WHISPER_SOURCE_DIR}/CMakeLists.txt")
  message(
    FATAL_ERROR
      "missing whisper.cpp checkout: ${ASRYX_WHISPER_SOURCE_DIR}. run ./package/install first."
  )
endif()

set(_ASRYX_SUPPORTED_BACKENDS cpu cuda vulkan)

if(NOT ASRYX_BACKEND IN_LIST _ASRYX_SUPPORTED_BACKENDS)
  message(
    FATAL_ERROR
      "unsupported ASRYX_BACKEND='${ASRYX_BACKEND}'; expected cpu, cuda, or vulkan"
  )
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
set(WHISPER_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(WHISPER_ALL_WARNINGS_3RD_PARTY OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_CURL OFF CACHE BOOL "" FORCE)
set(WHISPER_SDL2 OFF CACHE BOOL "" FORCE)
set(WHISPER_FFMPEG OFF CACHE BOOL "" FORCE)

if(ASRYX_BACKEND STREQUAL "cuda")
  set(GGML_CUDA ON CACHE BOOL "" FORCE)
elseif(ASRYX_BACKEND STREQUAL "vulkan")
  set(GGML_VULKAN ON CACHE BOOL "" FORCE)
endif()

message(STATUS "Asryx backend: ${ASRYX_BACKEND}")

add_subdirectory("${ASRYX_WHISPER_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/whisper.cpp" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE ASRYX_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ASRYX_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.hpp")

add_executable(asryx ${ASRYX_SOURCES} ${ASRYX_HEADERS})
target_include_directories(asryx PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(asryx PRIVATE ASRYX_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(asryx PRIVATE whisper)

asryx_enable_warnings(asryx)
asryx_enable_hardening(asryx)
asryx_enable_sanitizers(asryx)

if(ASRYX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
