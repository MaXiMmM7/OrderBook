cmake_minimum_required(VERSION 3.15)

project(OrderBook)

add_subdirectory(libs/oneTBB)

set(HEADERS
    include/Order.h
    include/ConcurrentOrderBook_HashSet.h
    include/ConcurrentOrderBook_HashMap.h
    include/TestClass.h)
set(SOURCES
    ${HEADERS}
    src/Main.cpp
    src/Order.cpp
    src/ConcurrentOrderBook_HashSet.cpp
    src/ConcurrentOrderBook_HashMap.cpp)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})


set(CMAKE_CXX_FLAGS "--std=c++20 -Wall -fsanitize=thread")
add_executable(order_book ${SOURCES})
target_include_directories(order_book
    PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/include")



target_link_libraries(order_book  TBB::tbb)


