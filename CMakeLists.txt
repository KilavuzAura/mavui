cmake_minimum_required(VERSION 3.20)

project(mavui LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets SerialPort PrintSupport REQUIRED)
find_package(Qt6 COMPONENTS WebEngineWidgets QUIET)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(mavui
    src/main.cpp
    src/settings.cpp
    src/connection.cpp
    src/utils.cpp
    src/logger.cpp
    src/logviewer.cpp
    src/csvmanager.cpp
    src/widgets.cpp
    src/sensorpub.cpp
    src/missionmanager.cpp
    include/settings.h
    include/connection.h
    include/utils.h
    include/logger.h
    include/logviewer.h
    include/csvmanager.h
    include/widgets.h
    include/sensorpub.h
    include/missionmanager.h
    resources.qrc 
)

target_include_directories(mavui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/c_library_v2
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mavui PRIVATE -Wno-address-of-packed-member)
endif()

target_link_libraries(mavui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::PrintSupport
)

if(Qt6WebEngineWidgets_FOUND)
    target_link_libraries(mavui PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(mavui PRIVATE USE_WEBENGINE)
endif()

if(WIN32)
    set_target_properties(mavui PROPERTIES WIN32_EXECUTABLE ON)
endif()

add_custom_command(TARGET mavui PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/config
)

file(COPY 
    "${CMAKE_CURRENT_SOURCE_DIR}/config/default.ini"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/map.json"
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/config
)