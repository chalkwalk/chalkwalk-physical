# chalkwalk-physical -- physical modelling: exciters, resonators, geometry.
#
# Written to work BOTH nested inside a parent project and standalone, so that
# `cmake -B build && ctest` works from this directory with nothing else on the
# machine. That is not a convenience: a library that only builds inside its
# parent has not actually been extracted, and the standalone build is the test
# of the boundary. See ../../../ECOSYSTEM.md.
#
# STRICTLY JUCE-FREE. A file is a host concern; this library must not learn
# what one is.

cmake_minimum_required(VERSION 3.22)
project(chalkwalk_physical VERSION 0.1.0 LANGUAGES CXX)

add_library(chalkwalk_physical STATIC
    src/WaveguideResonator.cpp
    src/ModalResonator.cpp
    src/ModalDataImporter.cpp
    src/Geometry.cpp
    src/StruckExciter.cpp
    src/BowedExciter.cpp
    src/TranslationMatrix.cpp)
add_library(chalkwalk::physical ALIAS chalkwalk_physical)

target_include_directories(chalkwalk_physical
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(chalkwalk_physical PUBLIC cxx_std_20)
set_target_properties(chalkwalk_physical PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(chalkwalk_physical PRIVATE /W4)
else()
    target_compile_options(chalkwalk_physical PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wimplicit-fallthrough)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CHALKWALK_PHYSICAL_TESTS_DEFAULT ON)
else()
    set(CHALKWALK_PHYSICAL_TESTS_DEFAULT OFF)
endif()
option(CHALKWALK_PHYSICAL_TESTS "Build chalkwalk-physical tests"
       ${CHALKWALK_PHYSICAL_TESTS_DEFAULT})

if(CHALKWALK_PHYSICAL_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
