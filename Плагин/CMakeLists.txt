cmake_minimum_required(VERSION 3.15)

project(zombie_infection CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    endstone
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../endstone-main/endstone-main/include"
)
FetchContent_MakeAvailable(endstone)

endstone_add_plugin(${PROJECT_NAME}
    src/zombie_plugin.cpp
    src/skin_service.cpp
    src/fire_service.cpp
    src/infection_manager.cpp
    src/mage_service.cpp
    src/packets/skin_packet.cpp
    src/packets/actor_data_packet.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
