project(master_clock LANGUAGES CXX)

link_directories(${CMAKE_BINARY_DIR})

add_executable(${PROJECT_NAME} src/master_clock.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET default)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FLATBUFFERS_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} fabrix)

