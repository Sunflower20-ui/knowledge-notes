cmake_minimum_required(VERSION 3.21)

project(KnowledgeNotes
    VERSION 0.1.0
    DESCRIPTION "Local knowledge note-taking application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Sql)

# Resources
set(RESOURCES
    ${CMAKE_SOURCE_DIR}/resource/resources.qrc
)

add_subdirectory(src)

if(KNOWLEDGENOTES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
