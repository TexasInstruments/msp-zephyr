# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)
FILE(GLOB app_sources src/*.c src/Display/*.c)
target_sources(app PRIVATE ${app_sources})
zephyr_compile_options(-mcmse)
zephyr_compile_options(-D__SAUREGION_PRESENT -DCONFIG_CPU_HAS_ARM_SAU)
zephyr_library_include_directories(${ZEPHYR_BASE}/samples/basic/blinky ${ZEPHYR_BASE}/samples/basic/blinky/src/Display /home/mspm33_SDK/msp-sdk/source/ /home/mspm33_SDK/msp-sdk//source/third_party/CMSIS/Core/Include/)
