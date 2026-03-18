# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bq27z8xx_i2c_stress)

target_sources(app PRIVATE src/test_i2c_stress.c)

# bq27z8xx buffer property enums and structs are in the driver-private header.
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/drivers/fuel_gauge/bq27z8xx)
