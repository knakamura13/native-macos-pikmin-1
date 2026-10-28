# Runs one scenario on the stand-in tree and fails on any `FAILED to open` line
# that the allowlist does not name (#68's generator pass criteria). Called by
# CTest as:
#   cmake -DGAME=<nectar> -DSCENARIO=<name> -DRATE=<30|60> -DALLOWLIST=<file> -P run_standin_scenario.cmake
# with PIKMIN_SCENARIO_DATA_DIR and PIKMIN_SCENARIO_STAGE in the test's
# environment. The scenario's own exit code is kept: anything but 0 fails.
cmake_minimum_required(VERSION 3.16)

foreach(var GAME SCENARIO RATE ALLOWLIST)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_standin_scenario.cmake: ${var} is not set")
    endif()
endforeach()

execute_process(
    COMMAND "${GAME}" --scenario "${SCENARIO}" --scenario-tick-rate "${RATE}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out
    RESULT_VARIABLE rc)
message("${out}")

# One allowlisted path per line (relative to assets/); a line ending in '/'
# covers every path under it; '#' starts a comment.
file(STRINGS "${ALLOWLIST}" allow_lines)
set(allowed)
foreach(line IN LISTS allow_lines)
    string(REGEX REPLACE "#.*" "" line "${line}")
    string(STRIP "${line}" line)
    if(line)
        list(APPEND allowed "${line}")
    endif()
endforeach()

string(REGEX MATCHALL "FAILED to open assets/[^\r\n]*" failed_lines "${out}")
set(unexpected)
foreach(line IN LISTS failed_lines)
    string(REGEX REPLACE "^FAILED to open assets/" "" path "${line}")
    string(STRIP "${path}" path)
    set(ok FALSE)
    foreach(entry IN LISTS allowed)
        if(entry STREQUAL path)
            set(ok TRUE)
        elseif(entry MATCHES "/$")
            string(LENGTH "${entry}" n)
            string(SUBSTRING "${path}" 0 ${n} head)
            if(head STREQUAL entry)
                set(ok TRUE)
            endif()
        endif()
    endforeach()
    if(NOT ok)
        list(APPEND unexpected "${path}")
    endif()
endforeach()
if(unexpected)
    list(REMOVE_DUPLICATES unexpected)
endif()

if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "scenario ${SCENARIO} at ${RATE} exited with '${rc}'")
endif()
if(unexpected)
    string(REPLACE ";" "\n  " shown "${unexpected}")
    message(FATAL_ERROR "scenario ${SCENARIO} at ${RATE}: files failed to open outside the allowlist:\n  ${shown}")
endif()
