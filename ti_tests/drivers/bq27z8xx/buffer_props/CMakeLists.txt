# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bq27z8xx_buffer_props)

target_sources(app PRIVATE src/test_bq27z8xx_buffer_props.c)

# BQ27Z8XX_BUFFER_PROP_* and BQ27Z855_BUFFER_PROP_* are in driver-private headers.
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/drivers/fuel_gauge/bq27z8xx)
