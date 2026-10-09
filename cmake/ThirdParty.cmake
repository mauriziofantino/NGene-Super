# Warning policy is private to external implementations and never propagated to NGene.
function(ngene_external_target target)
    set_target_properties(${target} PROPERTIES FOLDER "Dependencies")
    if(MSVC)
        # CMake maps /W0 to MSBuild WarningLevel=TurnOffAllWarnings. A raw /w lands
        # in AdditionalOptions, conflicting with MSBuild's default /W1 (D9025).
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:/W0>")
    else()
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:-w>")
    endif()
endfunction()
