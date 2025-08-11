cmake_minimum_required(VERSION 3.10)
project(RaptorQ_Project)

set(CMAKE_CXX_STANDARD 11)  # 或 11/14，根据你的代码情况

add_executable(Main
        Main.cpp
        Encoder.cpp
        Decoder.cpp
        Helper.cpp
        Symbol.cpp
        Generators.cpp
        StdAfx.cpp
)
