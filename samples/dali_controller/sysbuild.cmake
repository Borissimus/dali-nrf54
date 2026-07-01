set(DALI_FLPR_ENGINE_VARIANT "release" CACHE STRING
    "Select the prebuilt CPUFLPR DALI engine variant")
set_property(CACHE DALI_FLPR_ENGINE_VARIANT PROPERTY STRINGS release debug)

set(DALI_FLPR_ENGINE_CONF
    "${APP_DIR}/sysbuild/cpuflpr.conf")

if(DALI_FLPR_ENGINE_VARIANT STREQUAL "debug")
  set(DALI_FLPR_ENGINE_CONF
      "${DALI_FLPR_ENGINE_CONF};${APP_DIR}/sysbuild/cpuflpr_debug.conf")
elseif(NOT DALI_FLPR_ENGINE_VARIANT STREQUAL "release")
  message(FATAL_ERROR
    "Unsupported DALI_FLPR_ENGINE_VARIANT: ${DALI_FLPR_ENGINE_VARIANT}")
endif()

set(cpuflpr_EXTRA_CONF_FILE ${DALI_FLPR_ENGINE_CONF}
    CACHE INTERNAL "" FORCE)

ExternalZephyrProject_Add(
  APPLICATION cpuflpr
  SOURCE_DIR ${APP_DIR}/../../cpuflpr
  BOARD ${SB_CONFIG_BOARD}/${SB_CONFIG_SOC}/cpuflpr
  BOARD_REVISION ${BOARD_REVISION}
)

set_property(GLOBAL APPEND PROPERTY PM_DOMAINS CPUFLPR)
set_property(GLOBAL APPEND PROPERTY PM_CPUFLPR_IMAGES cpuflpr)
set_property(GLOBAL PROPERTY DOMAIN_APP_CPUFLPR cpuflpr)
set(CPUFLPR_PM_DOMAIN_DYNAMIC_PARTITION cpuflpr CACHE INTERNAL "")

add_dependencies(${DEFAULT_IMAGE} cpuflpr)
sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} cpuflpr)
