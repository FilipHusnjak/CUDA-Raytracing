cmake_minimum_required(VERSION 3.21)
project(CUDA_Raytracing CUDA)

set(CMAKE_CUDA_STANDARD 17)

add_executable(solution src/main.cu)
set_target_properties(solution PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)
