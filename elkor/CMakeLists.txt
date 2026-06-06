project(elkor LANGUAGES CXX VERSION 0.1.0 DESCRIPTION "Elkor metering")

link_directories(${CMAKE_BINARY_DIR})

add_executable(${PROJECT_NAME} src/elkor.cpp src/elkor_modbus.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET default)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${FLATBUFFERS_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} modbus fabrix)
add_dependencies(${PROJECT_NAME} fbs_schemas)
