cmake_minimum_required(VERSION 3.10.0)
project(ghost_window VERSION 0.1.0 LANGUAGES C CXX)

# --- Настройка для WINDOWS ---
if(WIN32)
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()

    if(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()

    # Собираем из оригинального файла main.cpp
    # Флаг WIN32 нужен, чтобы приложение считалось графическим (без консоли)
    add_executable(ghost_window WIN32 main.cpp)

    target_link_libraries(ghost_window PRIVATE user32)

# --- Настройка для LINUX (X11) ---
elseif(UNIX AND NOT APPLE)
    # Ищем пакеты X11
    find_package(X11 REQUIRED)
    
    find_library(XFIXES_LIB Xfixes)
    if(NOT XFIXES_LIB)
        message(FATAL_ERROR "Library Xfixes not found! Install libxfixes-dev.")
    endif()

    add_executable(ghost_window main_linux.cpp)

    target_include_directories(ghost_window PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(ghost_window PRIVATE ${X11_LIBRARIES} ${XFIXES_LIB})

endif()