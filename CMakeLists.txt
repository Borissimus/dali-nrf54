# External module entry point for Zephyr/NCS.

zephyr_include_directories(include)

add_subdirectory_ifdef(CONFIG_DALI_NRF54 drivers/dali)
