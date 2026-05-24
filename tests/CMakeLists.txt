file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

file(GLOB_RECURSE ASRYX_CORE_SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM ASRYX_CORE_SOURCES "${PROJECT_SOURCE_DIR}/src/main.cpp")

add_executable(asryx_tests ${TEST_SOURCES} ${ASRYX_CORE_SOURCES})
target_include_directories(
  asryx_tests
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(asryx_tests PRIVATE ASRYX_TESTING)
target_link_libraries(asryx_tests PRIVATE whisper)

asryx_enable_warnings(asryx_tests)
asryx_enable_hardening(asryx_tests)
asryx_enable_sanitizers(asryx_tests)

add_test(NAME unit_tests COMMAND asryx_tests)