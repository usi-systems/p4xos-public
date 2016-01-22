cmake_minimum_required (VERSION 2.8.4)
project (NetPaxos)

include_directories(include)
file(GLOB SOURCE_FILES src/*.c)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
find_package(LibEvent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})
set (EXTRA_LIBS ${EXTRA_LIBS}  ${LIBEVENT_LIBRARIES})

# print message when running cmake
message(STATUS "This is CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})
message(STATUS "Extra libs " ${EXTRA_LIBS})

add_executable (netpaxos ${SOURCE_FILES})
target_link_libraries (netpaxos ${EXTRA_LIBS})