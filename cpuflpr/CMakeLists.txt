cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dali_nrf54_flpr C)

set(DALI_FLPR_ENGINE_LIB_DIR
    ${CMAKE_CURRENT_LIST_DIR}/../zephyr/blobs/cpuflpr/lib/nrf54l15)
set(DALI_FLPR_ENGINE_LIB_RELEASE
    ${DALI_FLPR_ENGINE_LIB_DIR}/libdali_flpr_engine_release.a)
set(DALI_FLPR_ENGINE_LIB_DEBUG
    ${DALI_FLPR_ENGINE_LIB_DIR}/libdali_flpr_engine_debug.a)

if(CONFIG_DALI_FLPR_LOG_DEBUG)
  set(DALI_FLPR_ENGINE_LIB ${DALI_FLPR_ENGINE_LIB_DEBUG})
else()
  set(DALI_FLPR_ENGINE_LIB ${DALI_FLPR_ENGINE_LIB_RELEASE})
endif()

if(NOT EXISTS ${DALI_FLPR_ENGINE_LIB})
  message(FATAL_ERROR
    "Missing FLPR DALI engine archive: ${DALI_FLPR_ENGINE_LIB}\n"
    "Fetch module blobs with: west blobs fetch dali-nrf54")
endif()

add_library(dali_flpr_engine STATIC IMPORTED GLOBAL)
set_target_properties(dali_flpr_engine PROPERTIES
  IMPORTED_LOCATION ${DALI_FLPR_ENGINE_LIB}
)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../include
  ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(app PRIVATE dali_flpr_engine)

target_sources(app PRIVATE
  src/dali_heartbeat.c
  src/dali_ipc_server.c
  src/main.c
)
