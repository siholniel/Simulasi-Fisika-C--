   cmake_minimum_required(VERSION 3.14)
   project(atom3d CXX)

   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)

   # Supaya CMake baru mau membaca library lama
   set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

   include(FetchContent)
   FetchContent_Declare(
       raylib
       GIT_REPOSITORY https://github.com/raysan5/raylib.git
       GIT_TAG 5.5
   )
   set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
   FetchContent_MakeAvailable(raylib)

   add_executable(atom3d main.cpp)
   target_link_libraries(atom3d raylib)

   if (MSVC)
       target_compile_definitions(atom3d PRIVATE _USE_MATH_DEFINES)
   endif()