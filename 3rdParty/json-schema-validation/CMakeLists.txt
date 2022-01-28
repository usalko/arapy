cmake_minimum_required(VERSION 3.9)
project(arangodb-validation VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

### define static lib
add_library(arango-validation STATIC
    src/validation.cpp
)
target_compile_features(arango-validation PUBLIC cxx_std_20)
target_link_libraries(arango-validation PUBLIC
   valijson
   velocypack
)
target_include_directories(arango-validation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)


add_library(arango::validation ALIAS arango-validation)
