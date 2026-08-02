cmake_minimum_required(VERSION 3.25)

project(asryx LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Warnings)
include(Hardening)
include(Sanitizers)

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

add_subdirectory("${WHISPER_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/whisper.cpp" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.hpp")

add_executable(asryx ${SOURCES} ${HEADERS})
target_include_directories(asryx PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(
  asryx
  PRIVATE ASRYX_SOURCE_DIR="${PROJECT_SOURCE_DIR}" ${BACKEND_DEFINITION}
)
target_link_libraries(asryx PRIVATE whisper)
target_compile_options(asryx PRIVATE -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
target_link_options(asryx PRIVATE -Wl,--gc-sections)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
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
