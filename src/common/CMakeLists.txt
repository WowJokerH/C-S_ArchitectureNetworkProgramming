set(COMMON_SOURCES
    crc16.cpp
    protocol.cpp
    logger.cpp
)

add_library(protocol_lib STATIC ${COMMON_SOURCES})
target_include_directories(protocol_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protocol_lib PUBLIC Qt6::Core)
