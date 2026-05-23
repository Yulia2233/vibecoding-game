cmake_minimum_required(VERSION 3.24)

project(BluePlanetDefense LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

add_executable(blue_planet_defense
  src/main.cpp
  src/game.cpp
)

target_link_libraries(blue_planet_defense PRIVATE raylib)

if(APPLE)
  target_link_libraries(blue_planet_defense PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

add_custom_command(TARGET blue_planet_defense POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:blue_planet_defense>/assets
)
