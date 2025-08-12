cmake_minimum_required(VERSION 3.10)
project(RaptorQ_Project)

set(CMAKE_C_STANDARD 99)  # 使用C99标准

add_executable(Main
        Main.c
        Encoder.c
        Decoder.c
        Helper.c
        Symbol.c
        Generators.c
)

# 链接数学库
target_link_libraries(Main m)
