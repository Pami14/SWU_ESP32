cmake_minimum_required(VERSION 3.20)

project(silkwindup C CXX)

set(MCU atmega328p)
set(F_CPU 16000000UL)

set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)

add_executable(silkwindup.elf
    src/main.cpp
)

target_compile_definitions(silkwindup.elf PRIVATE
    F_CPU=${F_CPU}
)

target_compile_options(silkwindup.elf PRIVATE
    -mmcu=${MCU}
    -Os
)

target_link_options(silkwindup.elf PRIVATE
    -mmcu=${MCU}
)

add_custom_command(
    TARGET silkwindup.elf POST_BUILD
    COMMAND avr-objcopy
        -O ihex
        -R .eeprom
        silkwindup.elf
        silkwindup.hex
)