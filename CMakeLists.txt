cmake_minimum_required(VERSION 3.26)

# Added these 2 lines to disable tests and bypass the missing "Catch2" package
set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable generic CTest tests" FORCE)

project(
    ConsoleHelpAI
    VERSION 0.1.0
    DESCRIPTION "SKSE plugin that intercepts prefixed console input and sends it to AI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMONLIB_SSE_NG_DIR "D:/Games/Utilities/VS Projects-Dependencies/CommonLibSSE-NG")

if(NOT DEFINED COMMONLIB_SSE_NG_DIR)
    if(DEFINED ENV{COMMONLIB_SSE_NG_DIR})
        set(COMMONLIB_SSE_NG_DIR "$ENV{COMMONLIB_SSE_NG_DIR}")
    endif()
endif()

if(NOT COMMONLIB_SSE_NG_DIR)
    message(FATAL_ERROR
        "COMMONLIB_SSE_NG_DIR is not set. Point it to a local CommonLibSSE-NG checkout."
    )
endif()

add_subdirectory(${COMMONLIB_SSE_NG_DIR} CommonLibSSE-NG)

set(SOURCES
    src/main.cpp
    src/Settings.cpp
    src/ConsolePrinter.cpp
    src/AiClient.cpp
    src/ConsoleHook.cpp
)

set(HEADERS
    src/PCH.h
    src/Settings.h
    src/ConsolePrinter.h
    src/AiClient.h
    src/ConsoleHook.h
    src/Plugin.h
)

add_library(${PROJECT_NAME} SHARED
    ${SOURCES}
    ${HEADERS}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
        src/PCH.h
)

# WinHTTP keeps dependencies small and native to Windows.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CommonLibSSE::CommonLibSSE
        winhttp
)

# Output directly to a mod-like folder structure for easy testing.
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/SKSE/Plugins"
)

# Copy the sample ini files into the output folder after build.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/SKSE/Plugins/ConsoleHelpAI.ini"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/ConsoleHelpAI.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/SKSE/Plugins/ConsoleHelpAI_Key.ini"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/ConsoleHelpAI_Key.ini"
)
