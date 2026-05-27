set(SOURCES
    main.cpp
    app/main_window.h
    app/main_window.cpp
    core/database.h
    core/database.cpp
    core/note_repository.h
    core/note_repository.cpp
)

add_executable(knowledge_notes ${SOURCES})

target_include_directories(knowledge_notes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(knowledge_notes PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

set_target_properties(knowledge_notes PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
