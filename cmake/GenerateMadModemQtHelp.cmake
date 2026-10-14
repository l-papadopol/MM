# Generate MadModem Qt Help files in a build-safe, non-fatal way.
#
# qhelpgenerator is optional: MM always ships embedded multilingual HTML help.
# Some Qt/MXE qhelpgenerator builds can return a non-zero status even after
# printing "Documentation successfully generated", or can fail on one language
# while the remaining languages are usable.  Do not make the whole application
# build fail because compressed .qch generation failed.

if(NOT DEFINED HELP_SOURCE_DIR OR HELP_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "HELP_SOURCE_DIR is not set")
endif()
if(NOT DEFINED HELP_BINARY_DIR OR HELP_BINARY_DIR STREQUAL "")
    message(FATAL_ERROR "HELP_BINARY_DIR is not set")
endif()
if(NOT DEFINED QHELPGENERATOR_EXECUTABLE OR QHELPGENERATOR_EXECUTABLE STREQUAL "")
    message(WARNING "qhelpgenerator is not configured; skipping .qch generation")
    return()
endif()
if(NOT DEFINED HELP_LANGUAGES OR HELP_LANGUAGES STREQUAL "")
    set(HELP_LANGUAGES en it fr de no cs)
endif()

file(MAKE_DIRECTORY "${HELP_BINARY_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${HELP_SOURCE_DIR}" "${HELP_BINARY_DIR}"
    RESULT_VARIABLE _copy_result
    OUTPUT_VARIABLE _copy_stdout
    ERROR_VARIABLE  _copy_stderr
)
if(NOT _copy_result EQUAL 0)
    message(WARNING "Could not copy MM help sources to the build tree. HTML fallback in resources is still available.\n${_copy_stderr}")
    return()
endif()

set(_generated)
set(_failed)
set(_log "")

foreach(_lang IN LISTS HELP_LANGUAGES)
    set(_qhp "${HELP_BINARY_DIR}/MM_${_lang}.qhp")
    set(_qch "${HELP_BINARY_DIR}/MM_${_lang}.qch")

    if(NOT EXISTS "${_qhp}")
        list(APPEND _failed "${_lang}: missing ${_qhp}")
        continue()
    endif()

    # Avoid stale or locked partial output from earlier interrupted builds.
    file(REMOVE "${_qch}")

    execute_process(
        COMMAND "${QHELPGENERATOR_EXECUTABLE}" "${_qhp}" -o "${_qch}"
        WORKING_DIRECTORY "${HELP_BINARY_DIR}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE  _stderr
    )

    string(APPEND _log "===== MM_${_lang}.qch =====\n")
    if(NOT _stdout STREQUAL "")
        string(APPEND _log "${_stdout}\n")
    endif()
    if(NOT _stderr STREQUAL "")
        string(APPEND _log "${_stderr}\n")
    endif()

    if(EXISTS "${_qch}")
        file(SIZE "${_qch}" _qch_size)
    else()
        set(_qch_size 0)
    endif()

    if(_result EQUAL 0 AND _qch_size GREATER 0)
        list(APPEND _generated "MM_${_lang}.qch")
    elseif(_qch_size GREATER 0)
        # Keep the file: certain qhelpgenerator variants emit usable output but
        # still return a non-zero process status.  The runtime can use the .qch;
        # if it is bad, HelpDialog will fall back to embedded HTML.
        list(APPEND _generated "MM_${_lang}.qch (non-zero generator status ${_result}, kept because file exists)")
    else()
        list(APPEND _failed "${_lang}: qhelpgenerator status ${_result}")
    endif()
endforeach()

file(WRITE "${HELP_BINARY_DIR}/qhelpgenerator.log" "${_log}")

if(_generated)
    string(REPLACE ";" ", " _generated_msg "${_generated}")
    message(STATUS "Generated MM Qt Help: ${_generated_msg}")
endif()

if(_failed)
    string(REPLACE ";" "\n  - " _failed_msg "${_failed}")
    message(WARNING "Some MM .qch files were not generated. The build will continue and MM will use embedded multilingual HTML help where needed.\n  - ${_failed_msg}\nGenerator log: ${HELP_BINARY_DIR}/qhelpgenerator.log")
endif()
