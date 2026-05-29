set(SOURCES
    main.cpp
    app/main_window.h
    app/main_window.cpp
    app/markdown_highlighter.h
    app/markdown_highlighter.cpp
    core/database.h
    core/database.cpp
    core/note_repository.h
    core/note_repository.cpp
    core/search_result.h
)

add_executable(knowledge_notes ${SOURCES}
    ${RESOURCES}
)

target_include_directories(knowledge_notes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(knowledge_notes PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

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
