file(GLOB_RECURSE chat_handler_sources ${PROJECT_SOURCE_DIR}/chat_handler/*.cpp)

add_library(chat_handler SHARED ${chat_handler_sources})
target_include_directories(chat_handler PUBLIC
    ${PROJECT_SOURCE_DIR}/chat_handler/include
    ${PROJECT_SOURCE_DIR}/3rd/json
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)

target_include_directories(chat_handler PRIVATE ${MYSQL_INCLUDE_DIRS})
target_link_libraries(chat_handler PRIVATE ${MYSQL_LIBRARIES} pthread)