cmake_minimum_required(VERSION 3.10.0)
project(ghost_window VERSION 0.1.0 LANGUAGES C CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

if(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

add_executable(ghost_window WIN32 main.cpp)

target_link_libraries(ghost_window PRIVATE user32)

