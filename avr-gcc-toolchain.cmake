# avr-gcc-toolchain.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)
set(CMAKE_ASM_COMPILER avr-gcc)
set(CMAKE_OBJCOPY avr-objcopy)
set(CMAKE_SIZE avr-size)

# Avoid CMake trying to run test programs on the host
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

