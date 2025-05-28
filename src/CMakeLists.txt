cmake_minimum_required(VERSION 3.22.1)

project("fakesensor")

set(LIB_MAIN_NAME "yuuki")
set(LIB_HOOK_NAME "dobby_inject")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${LIB_MAIN_NAME} SHARED
        yuuki.cpp)

target_link_libraries(${LIB_MAIN_NAME}
        android
        log)

file(GLOB HOOK_SOURCES "hook/*.cpp")

add_library(${LIB_HOOK_NAME} SHARED
        ${HOOK_SOURCES})

target_link_libraries(${LIB_HOOK_NAME}
        android
        log
        dl
        ${CMAKE_SOURCE_DIR}/Dobby/${CMAKE_ANDROID_ARCH_ABI}/libdobby.a)

target_include_directories(${LIB_HOOK_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/hook)