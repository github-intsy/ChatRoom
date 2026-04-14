file(GLOB_RECURSE pine_sources ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(pine_shared SHARED ${pine_sources})

target_link_libraries(pine_shared PUBLIC chat_handler)