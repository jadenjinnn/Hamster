cmake_minimum_required(VERSION 3.28)
project(tinyfiledialogs)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


add_library(tinyfiledialogs STATIC hello.c hello_wchar_t.c tinyfiledialogs.c tinyfiledialogs.h)


