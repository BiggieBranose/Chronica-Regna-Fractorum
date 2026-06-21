@echo off
slangc shaders\shader.slang ^
    -target spirv ^
    -profile spirv_1_4 ^
    -emit-spirv-directly ^
    -entry vertMain -stage vertex ^
    -entry fragMain -stage fragment ^
    -o shaders\graphics.spv

REM compute shader not yet implemented
