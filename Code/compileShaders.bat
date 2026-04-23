@echo off
slangc shaders\shader.slang ^
    -target spirv ^
    -profile spirv_1_4 ^
    -emit-spirv-directly ^
    -fvk-use-entrypoint-name ^
    -entry vertMain ^
    -entry fragMain ^
    -o shaders/slang.spv
