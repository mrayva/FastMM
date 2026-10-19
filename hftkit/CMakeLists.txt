cmake_minimum_required(VERSION 3.20)
project(hftkit VERSION 0.1.0 DESCRIPTION "Small header-only building blocks and test tools for low-latency trading systems" LANGUAGES CXX)

add_library(hftkit INTERFACE)
add_library(hftkit::hftkit ALIAS hftkit)
target_include_directories(hftkit INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(hftkit INTERFACE cxx_std_20)

option(HFTKIT_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(HFTKIT_SANITIZE "Build the tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(HFTKIT_BUILD_TESTS)
  enable_testing()
  file(GLOB HFTKIT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp)
  foreach(src ${HFTKIT_TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE hftkit::hftkit)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    if(HFTKIT_SANITIZE)
      target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ${name} COMMAND ${name})
  endforeach()
endif()

install(DIRECTORY include/ DESTINATION include)
