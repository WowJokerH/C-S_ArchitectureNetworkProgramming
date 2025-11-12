set(SERVER_SOURCES
    main.cpp
    server_window.cpp
    session_worker.cpp
    listener.cpp
    connection_model.cpp
)

qt_add_executable(server_app
    MANUAL_FINALIZATION
    ${SERVER_SOURCES}
)

target_include_directories(server_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(server_app PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network protocol_lib)

set_target_properties(server_app PROPERTIES OUTPUT_NAME server)

qt_finalize_executable(server_app)
