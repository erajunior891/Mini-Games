cmake_minimum_required(VERSION 3.15)

project(world_border CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    endstone
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../endstone-main/endstone-main/include"
)
FetchContent_MakeAvailable(endstone)

endstone_add_plugin(${PROJECT_NAME}
    src/world_border_plugin.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
