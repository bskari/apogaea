cmake_minimum_required(VERSION 3.15)
project(piddle_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(sim
    main.cpp
    audio.cpp
    dsp.cpp
    renderer.cpp
    display.cpp
    ../esp32-fft.cpp
)

target_include_directories(sim PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(sim PRIVATE ${SDL2_LIBRARIES})

target_compile_options(sim PRIVATE -Wall -Wextra)
