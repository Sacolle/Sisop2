project(
  server
  VERSION 0.1
  DESCRIPTION "Código da aplicação do servidor"
  LANGUAGES CXX)


#src files for client
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(server ${SRC_FILES})

#.headers for lib -I
target_include_directories(server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(server PUBLIC lib)