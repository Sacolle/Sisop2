cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(trabalho_de_sisop_2 VERSION 0.1)

include(FetchContent)

add_subdirectory(lib)
add_subdirectory(client)
add_subdirectory(server)