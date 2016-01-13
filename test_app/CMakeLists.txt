cmake_minimum_required (VERSION 2.8.4)
project (NetPaxos)
set(SOURCE_FILES main.c learner.c learner.h proposer.c proposer.h message.c message.h)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(Boost COMPONENTS program_options REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
add_definitions (-DBOOST_TEST_DYN_LINK)
set (EXTRA_LIBS ${EXTRA_LIBS}  ${Boost_LIBRARIES})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
find_package(LibEvent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})
set (EXTRA_LIBS ${EXTRA_LIBS}  ${LIBEVENT_LIBRARIES})

# print message when running cmake
message(STATUS "This is CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})
message(STATUS "Extra libs " ${EXTRA_LIBS})

add_executable (netpaxos ${SOURCE_FILES})
target_link_libraries (netpaxos ${EXTRA_LIBS})