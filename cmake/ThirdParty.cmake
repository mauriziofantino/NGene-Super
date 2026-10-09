# Warnings are disabled only while compiling external implementations. Never export /w.
function(ngene_external_target target)
    set_target_properties(${target} PROPERTIES FOLDER "Dependencies")
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:/w>")
    else()
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:-w>")
    endif()
endfunction()
