set(CLIENT_SOURCES
    main.cpp
    client_window.cpp
    client_controller.cpp
)

qt_add_executable(client_app
    MANUAL_FINALIZATION
    ${CLIENT_SOURCES}
)

target_include_directories(client_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(client_app PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network protocol_lib)

set_target_properties(client_app PROPERTIES OUTPUT_NAME client)

qt_finalize_executable(client_app)
