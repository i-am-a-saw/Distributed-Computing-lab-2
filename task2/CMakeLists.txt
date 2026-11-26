cmake_minimum_required(VERSION 3.16)

project(ConvexHull LANGUAGES CXX)

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск OpenMP
find_package(OpenMP REQUIRED)

# Исходные файлы
add_executable(convex_hull
    src/main.cpp
    src/geometry.cpp
    src/preparata_hong.cpp
)

# Подключение заголовочных файлов
target_include_directories(convex_hull PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

# Подключение OpenMP
target_link_libraries(convex_hull PRIVATE
    OpenMP::OpenMP_CXX
)

# Оптимизация
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(convex_hull PRIVATE -O3)
endif()
