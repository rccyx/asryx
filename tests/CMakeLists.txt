list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/share/asryx/deps/libassert/install")

find_package(libassert CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

file(GLOB_RECURSE CORE_SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")
set(TEST_EXCLUDED_SOURCES
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/engine.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/output.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/recorder/recorder.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/transcription/transcription.cpp")
list(REMOVE_ITEM CORE_SOURCES ${TEST_EXCLUDED_SOURCES})

add_executable(asryx_tests ${TEST_SOURCES} ${CORE_SOURCES})
target_include_directories(
  asryx_tests
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(asryx_tests PRIVATE ${BACKEND_DEFINITION})
target_link_libraries(asryx_tests PRIVATE libassert::assert)

enable_warnings(asryx_tests)
enable_hardening(asryx_tests)
enable_sanitizers(asryx_tests)

add_test(NAME unit_tests COMMAND asryx_tests)
