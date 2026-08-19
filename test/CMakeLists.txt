# Catch2, guarded so a parent that already provides it is reused rather than
# fetching a second copy.
if(NOT TARGET Catch2::Catch2WithMain)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1)
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(chalkwalk_physical_tests
    WaveguideResonatorTests.cpp
    StruckExciterTests.cpp
    BowedExciterTests.cpp
    ModalResonatorTests.cpp
    GeometryTests.cpp
    TranslationMatrixTests.cpp
    TuningTests.cpp)

target_link_libraries(chalkwalk_physical_tests
    PRIVATE chalkwalk::physical Catch2::Catch2WithMain)

if(MSVC)
    target_compile_options(chalkwalk_physical_tests PRIVATE /W4)
else()
    target_compile_options(chalkwalk_physical_tests
        PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

add_test(NAME chalkwalk_physical COMMAND chalkwalk_physical_tests)
