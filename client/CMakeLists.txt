project(
  client
  VERSION 0.1
  DESCRIPTION "Código da aplicação do cliente"
  LANGUAGES CXX)


#src files for client
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(client ${SRC_FILES})

#.headers for lib -I
target_include_directories(client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(client PUBLIC lib)