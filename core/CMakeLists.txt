# etlimit_core: the DSP engine. Plain C++20, no JUCE, no allocation or
# locking inside any process() function.

add_library(etlimit_core STATIC
    src/Engine.cpp
)

target_include_directories(etlimit_core PUBLIC include)
target_link_libraries(etlimit_core PRIVATE etlimit_warnings)
target_compile_features(etlimit_core PUBLIC cxx_std_20)
