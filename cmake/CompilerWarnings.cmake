# Enable modern compiler warnings for a target.
#
# Usage:
#   include(CompilerWarnings)
#   enable_project_warnings(my_target)
#
# Warnings are applied as PRIVATE compile options so they do not propagate
# to consumers that link against the target.

function(enable_project_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set(_gcc_clang_warnings
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wimplicit-fallthrough
            -Wnull-dereference
            -Woverloaded-virtual
        )

        target_compile_options(${target_name} PRIVATE ${_gcc_clang_warnings})

        # Clang-specific additional warnings
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
            target_compile_options(${target_name} PRIVATE -Wshadow)
        endif()

    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    endif()
endfunction()
