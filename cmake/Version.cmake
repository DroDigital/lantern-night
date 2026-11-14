# Version from `git describe`: tag v1.2.3 -> "1.2.3", otherwise "1.2.3-N-gHASH" or "0.0.0-gHASH".
set(LK_VERSION "0.0.0-dev")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*" --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE LK_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE LK_GIT_RESULT)
    if(LK_GIT_RESULT EQUAL 0 AND LK_GIT_DESCRIBE)
        if(LK_GIT_DESCRIBE MATCHES "^v([0-9].*)$")
            set(LK_VERSION "${CMAKE_MATCH_1}")
        else()
            set(LK_VERSION "0.0.0-g${LK_GIT_DESCRIBE}")
        endif()
    endif()
endif()
# Numeric part for Info.plist / Windows resources.
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" LK_VERSION_NUMERIC "${LK_VERSION}")
if(NOT LK_VERSION_NUMERIC)
    set(LK_VERSION_NUMERIC "0.0.0")
endif()
message(STATUS "Lantern Knight version: ${LK_VERSION}")
