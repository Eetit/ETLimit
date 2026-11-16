# Tests.
#   etlimit_unit_tests : fast C++ tests of the DSP core (Catch2)
#   etlimit_render     : offline tool, WAV in -> engine -> WAV out. The Python
#                        reference measurements drive this tool, so DSP results
#                        are checked by code that shares nothing with the plugin.

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_library(etlimit_testcommon STATIC common/WavFile.cpp)
target_include_directories(etlimit_testcommon PUBLIC common)
target_link_libraries(etlimit_testcommon PRIVATE etlimit_warnings)

add_executable(etlimit_unit_tests
    unit/TestLinearSmoother.cpp
    unit/TestDelayLine.cpp
    unit/TestEngine.cpp
    unit/TestWavFile.cpp)
target_link_libraries(etlimit_unit_tests PRIVATE etlimit_core etlimit_testcommon etlimit_warnings Catch2::Catch2WithMain)
catch_discover_tests(etlimit_unit_tests)

add_executable(etlimit_render render/main.cpp)
target_link_libraries(etlimit_render PRIVATE etlimit_core etlimit_testcommon etlimit_warnings)

# Independent Python reference checks (need numpy, scipy, soundfile).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME reference_selfcheck
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/reference/selfcheck_truepeak.py
                --out ${CMAKE_BINARY_DIR}/qa/reference_selfcheck.json)
    add_test(NAME reference_stage1
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/reference/run_stage1.py
                --render $<TARGET_FILE:etlimit_render>
                --work ${CMAKE_BINARY_DIR}/qa/stage1
                --out ${CMAKE_BINARY_DIR}/qa/stage1.json)
endif()
