cmake_minimum_required(VERSION 3.13)

project(GAME)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()

include(FetchContent)
# TODO: Use SDL2 2.0.11 once released
FetchContent_Declare(
  SDL
  URL https://hg.libsdl.org/SDL/archive/a5f93b21dfe0.tar.gz # rev 13383
  URL_HASH SHA1=a7e9696f7ef8af7c4b89671b096c38d1a4cbf90c
  PATCH_COMMAND patch -p1 -i ${CMAKE_CURRENT_LIST_DIR}/SDL.patch
)
# TODO: Use SDL2_ttf 2.0.16 once released
FetchContent_Declare(
  SDL_ttf
  URL https://hg.libsdl.org/SDL_ttf/archive/d56d67db41d8.tar.gz # rev 420
  URL_HASH SHA1=143089a2b2973a5baeee0914af43608cfe8e1d87
)

# Compilation of SDL and companion libraries
FetchContent_GetProperties(SDL)
if(NOT sdl_POPULATED)
  FetchContent_Populate(SDL)
  add_subdirectory(${sdl_SOURCE_DIR} ${sdl_BINARY_DIR})
endif()
FetchContent_GetProperties(SDL_ttf)
if(NOT sdl_ttf_POPULATED)
  FetchContent_Populate(SDL_ttf)
  add_subdirectory(${sdl_ttf_SOURCE_DIR} ${sdl_ttf_BINARY_DIR})
endif()

# The main CMakeLists.txt of system3
add_subdirectory(../../../src src)