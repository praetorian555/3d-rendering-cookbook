function(setup_compiler_options target)

    target_compile_options(${target} INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
    target_compile_options(${target} INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

endfunction()