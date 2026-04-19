cmake_minimum_required(VERSION 3.20)

project(liryna
    VERSION 0.1.0
    DESCRIPTION "A C game project"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE LIRYNA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

if(NOT LIRYNA_SOURCES)
    message(FATAL_ERROR "No C source files found. Add at least one .c file under src/.")
endif()

add_executable(liryna ${LIRYNA_SOURCES})

target_include_directories(liryna
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(liryna PRIVATE /W4)
else()
    target_compile_options(liryna PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(liryna PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(CTest)

file(GLOB_RECURSE LIRYNA_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c"
)

if(BUILD_TESTING AND LIRYNA_TEST_SOURCES)
    add_executable(liryna_tests ${LIRYNA_TEST_SOURCES})

    target_include_directories(liryna_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    if(MSVC)
        target_compile_options(liryna_tests PRIVATE /W4)
    else()
        target_compile_options(liryna_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME liryna_tests COMMAND liryna_tests)
endif()
