cmake_minimum_required(VERSION 3.16)
project(path_planning_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

add_executable(path_planning_sim
  src/main.cpp
)

target_link_libraries(path_planning_sim PRIVATE
  SDL2::SDL2
  SDL2::SDL2main
  SDL2_ttf::SDL2_ttf
)

if (WIN32)
  target_compile_definitions(path_planning_sim PRIVATE SDL_MAIN_HANDLED)
endif()
