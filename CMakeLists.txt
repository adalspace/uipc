cmake_minimum_required(VERSION 3.20)
set(UIPC "uipc")
set(UIPC_CPP "uipc-cpp")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${UIPC} LANGUAGES C CXX)

add_library(${UIPC} STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/uipc.c ${CMAKE_CURRENT_SOURCE_DIR}/src/upack.c)
target_include_directories(${UIPC} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${UIPC_CPP} INTERFACE)
target_include_directories(${UIPC_CPP} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${UIPC_CPP} INTERFACE ${UIPC})

add_library(uipc::c ALIAS ${UIPC})
add_library(uipc::cpp ALIAS ${UIPC_CPP})

include(CTest)
add_subdirectory(sandbox)
