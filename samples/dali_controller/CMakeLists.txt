# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026 N-iX

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dali_controller)

target_sources(app PRIVATE
  src/cpuapp_heartbeat.c
  src/main.c
)

target_sources_ifdef(CONFIG_DALI_CPU_LOAD_REPORTING app PRIVATE
  src/cpu_load_reporter.c
)

target_sources_ifdef(CONFIG_DALI_BLE_LOAD app PRIVATE
  src/ble_load.c
)
