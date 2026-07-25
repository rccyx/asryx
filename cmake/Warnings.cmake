function(enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    return()
  endif()

  target_compile_options(
    ${target}
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wformat=2
      -Wnull-dereference
      -Wdouble-promotion
      -Wimplicit-fallthrough
      -Wold-style-cast
      -Woverloaded-virtual
      -Wnon-virtual-dtor
      -Wcast-align
      -Wcast-qual
      -Wmissing-declarations
      -Wredundant-decls
      -Wundef
      -Wunused
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE -Wunreachable-code -Wextra-semi)
  endif()
endfunction()
