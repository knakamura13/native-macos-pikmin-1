# Runs boot_smoke with --scenario-screenshot and checks the PNG (issue #87 P0).
cmake_minimum_required(VERSION 3.16)

foreach(var GAME WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "check_scenario_screenshot.cmake: ${var} is not set")
    endif()
endforeach()

set(pos_png "${WORK_DIR}/boot_smoke_t100.png")
set(neg_png "${WORK_DIR}/never_t99999.png")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${GAME}" --scenario boot_smoke --scenario-tick-rate 30
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
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/verify_scenario_png.py"
        "${pos_png}" --expect-width 128 --expect-height 72 --require-non-blank
    RESULT_VARIABLE pos_rc)
if(NOT pos_rc STREQUAL "0")
    message(FATAL_ERROR "positive PNG check failed")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/verify_scenario_png.py"
        "${neg_png}" --expect-width 128 --expect-height 72 --require-missing
    RESULT_VARIABLE neg_rc)
if(NOT neg_rc STREQUAL "0")
    message(FATAL_ERROR "negative PNG check failed (file should be absent for unreachable tick)")
endif()

message("scenario screenshot checks passed")
