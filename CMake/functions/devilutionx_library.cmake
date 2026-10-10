include(functions/genex)
include(functions/set_relative_file_macro)
include(functions/object_libraries)

# This function is equivalent to `add_library` but applies DevilutionX-specific
# compilation flags to it.
function(add_devilutionx_library NAME)
  add_library(${NAME} ${ARGN})

  target_include_directories(${NAME} PUBLIC ${PROJECT_SOURCE_DIR}/Source)

  target_compile_definitions(${NAME} PUBLIC ${DEVILUTIONX_PLATFORM_COMPILE_DEFINITIONS})
  target_compile_options(${NAME} PUBLIC ${DEVILUTIONX_PLATFORM_COMPILE_OPTIONS})

  genex_for_option(DEBUG)
  target_compile_definitions(${NAME} PUBLIC "$<${DEBUG_GENEX}:_DEBUG>")

  if(NOT NONET AND NOT DISABLE_TCP)
    target_compile_definitions(${NAME} PUBLIC ASIO_STANDALONE)
  endif()

  genex_for_option(UBSAN)
  target_compile_options(${NAME} PUBLIC $<${UBSAN_GENEX}:-fsanitize=undefined>)
  target_link_libraries(${NAME} PUBLIC $<${UBSAN_GENEX}:-fsanitize=undefined>)

  if(TSAN)
    target_compile_options(${NAME} PUBLIC -fsanitize=thread)
    target_link_libraries(${NAME} PUBLIC -fsanitize=thread)
  else()
    genex_for_option(ASAN)
    target_compile_options(${NAME} PUBLIC "$<${ASAN_GENEX}:-fsanitize=address;-fsanitize-recover=address>")
    target_link_libraries(${NAME} PUBLIC "$<${ASAN_GENEX}:-fsanitize=address;-fsanitize-recover=address>")
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    genex_for_option(DEVILUTIONX_STATIC_CXX_STDLIB)
    target_link_libraries(${NAME} PUBLIC $<${DEVILUTIONX_STATIC_CXX_STDLIB_GENEX}:-static-libgcc;-static-libstdc++>)
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Note: For Valgrind support.
    genex_for_option(DEBUG)
    target_compile_options(${NAME} PUBLIC $<${DEBUG_GENEX}:-fno-omit-frame-pointer>)

    # Warnings for devilutionX
    target_compile_options(${NAME} PUBLIC -Wall -Wextra -Wno-unused-parameter)
  endif()

  if(NOT WIN32 AND NOT APPLE AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
    # Enable POSIX extensions such as `readlink` and `ftruncate`.
    add_definitions(-D_POSIX_C_SOURCE=200809L)
  endif()

  if(BUILD_TESTING)
    if(ENABLE_CODECOVERAGE)
      if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        message(WARNING "Codecoverage not supported with MSVC")
      else()
        target_compile_options(${NAME} PUBLIC --coverage)
        target_link_options(${NAME} PUBLIC --coverage)
      endif()
    endif()

    target_compile_definitions(${NAME} PRIVATE _DVL_EXPORTING)
  endif()

  target_compile_definitions(${NAME} PUBLIC ${DEVILUTIONX_DEFINITIONS})

  # Ensure all libraries and the shared PCH compile with a consistent threading
  # model (e.g. -pthread / _REENTRANT) and transitively provide threading flags.
  target_link_libraries(${NAME} PUBLIC Threads::Threads)

  set_relative_file_macro(${NAME})
endfunction()

# Same as add_devilutionx_library(${NAME} OBJECT).
#
# Object libraries additionally share the precompiled header owned by
# `libdevilutionx_pch` (see Source/pch.hpp). They all compile with the same
# code generation model and with a superset of that target's defines, which is
# what makes a single shared PCH valid for every one of them. Executables are
# excluded: they build with a different PIC/PIE model, which GCC treats as
# invalidating the PCH.
#
# `DEVILUTIONX_PCH_SHARED` is off for compilers that reject a PCH built with
# anything other than the consumer's exact flags; there libdevilutionx gets its
# own PCH instead of everyone sharing one.
function(add_devilutionx_object_library NAME)
  add_devilutionx_library(${NAME} OBJECT ${ARGN})
  if(DEVILUTIONX_PCH)
    # pch.hpp is C++, and so is the precompiled header built from it. Some
    # platforms add sources in other languages (C on Amiga, Objective-C on
    # iOS); without this CMake looks for a C precompiled header that no target
    # ever builds and fails to generate.
    foreach(_src ${ARGN})
      if(NOT _src MATCHES "\\.(cpp|cxx|cc)$")
        set_source_files_properties(${_src} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
      endif()
    endforeach()
  endif()
  if(DEVILUTIONX_PCH_SHARED AND NOT NAME STREQUAL "libdevilutionx_pch")
    target_precompile_headers(${NAME} REUSE_FROM libdevilutionx_pch)
  endif()
endfunction()
