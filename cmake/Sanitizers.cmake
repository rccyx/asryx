set(SANITIZER "" CACHE STRING "Sanitizer profile: address, undefined, or empty")
set_property(CACHE SANITIZER PROPERTY STRINGS "" "address" "undefined")

function(enable_sanitizers target)
  if(NOT SANITIZER)
    return()
  endif()

  if(MSVC)
    message(FATAL_ERROR "Sanitizer presets are configured for GCC/Clang on Linux.")
  endif()

  if(SANITIZER STREQUAL "address")
    # catches memory misuse such as buffer overflows and use-after-free.
    target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address)
    return()
  endif()

  if(SANITIZER STREQUAL "undefined")
    # catches undefined behavior that may compile but still corrupt program semantics.
    target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=undefined)
    return()
  endif()

  message(FATAL_ERROR "Unknown sanitizer profile: ${SANITIZER}")
endfunction()
