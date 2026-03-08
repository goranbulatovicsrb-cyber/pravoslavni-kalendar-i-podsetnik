cmake_minimum_required(VERSION 3.16)
project(OrthodoxReminderProV3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WIN32_EXECUTABLE ON)

add_executable(OrthodoxReminderPro WIN32
    src/main.cpp
    resources/app.rc
)

target_link_libraries(OrthodoxReminderPro PRIVATE comctl32 shell32)

if (MSVC)
    target_compile_options(OrthodoxReminderPro PRIVATE /W3 /EHsc /utf-8)
else()
    target_compile_options(OrthodoxReminderPro PRIVATE -Wall -Wextra)
endif()
