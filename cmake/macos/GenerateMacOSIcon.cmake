if(NOT INPUT_ICON OR NOT EXISTS "${INPUT_ICON}")
    message(FATAL_ERROR "INPUT_ICON not found: ${INPUT_ICON}")
endif()
if(NOT OUTPUT_ICON)
    message(FATAL_ERROR "OUTPUT_ICON is not set")
endif()

find_program(SIPS_EXECUTABLE sips)
find_program(ICONUTIL_EXECUTABLE iconutil)
if(NOT SIPS_EXECUTABLE OR NOT ICONUTIL_EXECUTABLE)
    message(FATAL_ERROR "macOS icon generation needs sips and iconutil")
endif()

get_filename_component(_out_dir "${OUTPUT_ICON}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")
set(_iconset "${_out_dir}/MadModem.iconset")
file(REMOVE_RECURSE "${_iconset}")
file(MAKE_DIRECTORY "${_iconset}")

function(_madmodem_make_icon px name)
    execute_process(
        COMMAND "${SIPS_EXECUTABLE}" -z ${px} ${px} "${INPUT_ICON}" --out "${_iconset}/${name}"
        RESULT_VARIABLE _res
        OUTPUT_QUIET
        ERROR_VARIABLE _err
    )
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "sips failed for ${name}: ${_err}")
    endif()
endfunction()

_madmodem_make_icon(16   "icon_16x16.png")
_madmodem_make_icon(32   "icon_16x16@2x.png")
_madmodem_make_icon(32   "icon_32x32.png")
_madmodem_make_icon(64   "icon_32x32@2x.png")
_madmodem_make_icon(128  "icon_128x128.png")
_madmodem_make_icon(256  "icon_128x128@2x.png")
_madmodem_make_icon(256  "icon_256x256.png")
_madmodem_make_icon(512  "icon_256x256@2x.png")
_madmodem_make_icon(512  "icon_512x512.png")
_madmodem_make_icon(1024 "icon_512x512@2x.png")

execute_process(
    COMMAND "${ICONUTIL_EXECUTABLE}" -c icns "${_iconset}" -o "${OUTPUT_ICON}"
    RESULT_VARIABLE _iconutil_res
    ERROR_VARIABLE _iconutil_err
)
if(NOT _iconutil_res EQUAL 0)
    message(FATAL_ERROR "iconutil failed: ${_iconutil_err}")
endif()
