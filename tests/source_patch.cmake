cmake_minimum_required(VERSION 3.26)
include("${source}/cmake/ApplySourcePatch.cmake")
if(negative)
    ngene_apply_source_patch("${fixture}/invalid.patch" "${fixture}")
    message(FATAL_ERROR "Invalid context was accepted")
endif()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef unique)
set(fixture "${work}/source patch ${unique}")
file(MAKE_DIRECTORY "${fixture}")
set(original [=[int first = 0;
const char *literal = "${keep};@KEEP@";
int last = 0;
]=])
set(expected [=[int first = 1;
const char *literal = "${keep};@KEEP@";
int last = 2;
]=])
file(WRITE "${fixture}/valid.patch" [=[--- a/first.c
+++ b/first.c
@@ -1 +1 @@
-int first = 0;
+int first = 1;
@@ -3 +3 @@
-int last = 0;
+int last = 2;
--- a/second.h
+++ b/second.h
@@ -1 +1 @@
-#define VALUE 0
+#define VALUE 3
]=])
foreach(pass RANGE 1 20)
    file(WRITE "${fixture}/first.c" "${original}")
    file(WRITE "${fixture}/second.h" "#define VALUE 0\n")
    ngene_apply_source_patch("${fixture}/valid.patch" "${fixture}")
    file(READ "${fixture}/first.c" actual)
    file(READ "${fixture}/second.h" header)
    if(NOT actual STREQUAL expected OR NOT header STREQUAL "#define VALUE 3\n")
        message(FATAL_ERROR "Multi-file/multi-hunk patch changed literal source text")
    endif()
endforeach()
file(WRITE "${fixture}/invalid.patch" [=[--- a/first.c
+++ b/first.c
@@ -1 +1 @@
-int first = 1;
+int first = 4;
@@ -3 +3 @@
-missing context
+unexpected replacement
]=])
execute_process(COMMAND "${CMAKE_COMMAND}" "-Dsource=${source}" "-Dfixture=${fixture}"
    -Dnegative=TRUE -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(result EQUAL 0 OR NOT errors MATCHES "patch context not found")
    message(FATAL_ERROR "Invalid patch did not fail as expected: ${output}\n${errors}")
endif()
file(READ "${fixture}/first.c" actual)
file(GLOB leftovers "${fixture}/*.ngene-patch-*")
if(NOT actual STREQUAL expected OR leftovers)
    message(FATAL_ERROR "Failed patch modified its target or left temporary files")
endif()
message(STATUS "PASS: repeated atomic source patches, multiple files/hunks, literal preservation and failure safety")
