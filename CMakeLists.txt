cmake_minimum_required(VERSION 3.0)

project(MeterForPulseAudio VERSION 1.0)

set(MeterForPulseAudio_SOURCES
    src/Main.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -D NDEBUG")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug', none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

add_executable(MeterForPulseAudio ${MeterForPulseAudio_SOURCES})

if(APPEND_VERSION_TO_EXECUTABLE)
    set_target_properties(MeterForPulseAudio
        PROPERTIES
        VERSION ${MeterForPulseAudio_VERSION}
    )
endif()

target_compile_features(MeterForPulseAudio PUBLIC cxx_std_14)

find_package(PulseAudio 11 REQUIRED)
message(STATUS "Found PulseAudio ${PulseAudio_VERSION}")

target_include_directories(MeterForPulseAudio PUBLIC ${PULSEAUDIO_INCLUDE_DIR})
target_link_libraries(MeterForPulseAudio PUBLIC ${PULSEAUDIO_LIBRARY})

# add sub-project AnotherDangParser
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/AnotherDangParser/src")
    message(FATAL_ERROR "AnotherDangParser submodule not loaded! Please run "
        "'git submodule update --init --recursive'!")
endif()
add_subdirectory(AnotherDangParser)
target_include_directories(MeterForPulseAudio PUBLIC AnotherDangParser/src)
target_link_libraries(MeterForPulseAudio PUBLIC AnotherDangParser)

install(TARGETS MeterForPulseAudio
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
