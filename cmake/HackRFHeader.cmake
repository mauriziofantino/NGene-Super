# Upstream export declarations stay on the DLL target. Consumers use the same ABI with dllimport.
function(ngene_hackrf_consumer_header input output)
    file(READ "${input}" header)
    string(REGEX REPLACE "(#ifdef _WIN32)[\r\n\t ]*#define ADD_EXPORTS[\r\n]*" "\\1\n"
        header "${header}")
    get_filename_component(directory "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    file(WRITE "${output}" "${header}")
endfunction()
