list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/opt/libassert/install")

find_package(libassert CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

file(GLOB_RECURSE ASRYX_CORE_SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")
set(ASRYX_TEST_EXCLUDED_SOURCES
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/audio/audio.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/engine.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/output.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/recorder/recorder.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/transcription/transcription.cpp")
list(REMOVE_ITEM ASRYX_CORE_SOURCES ${ASRYX_TEST_EXCLUDED_SOURCES})

add_executable(asryx_tests ${TEST_SOURCES} ${ASRYX_CORE_SOURCES})
target_include_directories(
  asryx_tests
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(asryx_tests PRIVATE ${ASRYX_BACKEND_DEFINITION})
target_link_libraries(asryx_tests PRIVATE libassert::assert)

asryx_enable_warnings(asryx_tests)
asryx_enable_hardening(asryx_tests)
asryx_enable_sanitizers(asryx_tests)

add_test(NAME unit_tests COMMAND asryx_tests)
