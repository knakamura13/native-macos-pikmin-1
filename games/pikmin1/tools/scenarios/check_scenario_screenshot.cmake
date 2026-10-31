# Runs boot_smoke with --scenario-screenshot and checks the PNG (issue #87).
cmake_minimum_required(VERSION 3.16)

foreach(var GAME WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "check_scenario_screenshot.cmake: ${var} is not set")
    endif()
endforeach()

set(capture_w 640)
set(capture_h 480)
set(pos_png "${WORK_DIR}/boot_smoke_t100.png")
set(neg_png "${WORK_DIR}/never_t99999.png")
file(MAKE_DIRECTORY "${WORK_DIR}")

# Run at 640x480 for the whole scenario. A mid-tick resize from the default 64x36
# window enlarges the native FBO on the capture tick but headless CI (llvmpipe /
# Apple GL) often read back a cleared 640x480 target (~12 distinct colours).
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env PIKMIN_SCENARIO_WINDOW=640x480
        "${GAME}" --scenario boot_smoke --scenario-tick-rate 30
        --scenario-screenshot "100:${pos_png}"
        --scenario-screenshot "99999:${neg_png}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE rc)
message("${out}${err}")

if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "boot_smoke with screenshot exited ${rc}")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(verify "${CMAKE_CURRENT_LIST_DIR}/verify_scenario_png.py")
set(golden "${CMAKE_CURRENT_LIST_DIR}/golden/boot_smoke_tick100_640x480.png")

set(common_verify
    --expect-width ${capture_w}
    --expect-height ${capture_h}
    --require-non-blank
    --min-distinct-colors 80
    --min-variance 80.0)

set(verify_pos_cmd "${Python3_EXECUTABLE}" "${verify}" "${pos_png}" ${common_verify})
if(DEFINED GOLDEN_COMPARE)
    list(APPEND verify_pos_cmd
        --golden "${golden}" --golden-tolerance 10 --golden-max-diff-ratio 0.03)
endif()

execute_process(
    COMMAND ${verify_pos_cmd}
    RESULT_VARIABLE pos_rc)
if(NOT pos_rc STREQUAL "0")
    message(FATAL_ERROR "positive PNG check failed")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${verify}" "${neg_png}"
        --expect-width ${capture_w} --expect-height ${capture_h} --require-missing
    RESULT_VARIABLE neg_rc)
if(NOT neg_rc STREQUAL "0")
    message(FATAL_ERROR "negative PNG check failed (file should be absent for unreachable tick)")
endif()

message("scenario screenshot checks passed")
