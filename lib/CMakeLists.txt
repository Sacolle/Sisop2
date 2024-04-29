
project(
  lib
  VERSION 0.1
  DESCRIPTION "Código compartilhado"
  LANGUAGES CXX)


#src files for client
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(lib ${SRC_FILES})

#.headers for lib -I
target_include_directories(lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

FetchContent_Declare(
	flatbuffers
	GIT_REPOSITORY https://github.com/google/flatbuffers.git
	GIT_TAG v24.3.25
)

FetchContent_MakeAvailable(flatbuffers)

target_link_libraries(lib PUBLIC flatbuffers) 

#adicionando os schemas de mensagem
file(GLOB_RECURSE SCHEMA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/schemas/*.fbs)

flatbuffers_generate_headers(
	TARGET project_schemas
	SCHEMAS ${SCHEMA_FILES}
)

target_link_libraries(lib PUBLIC project_schemas) 
