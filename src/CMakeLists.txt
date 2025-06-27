zephyr_syscall_header(../include/drivers/at-modem.h)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MODEM_SIM7600G simcom-sim7600g.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_A7682E simcom-a7682e.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_ESP32C3_AT esp32c3-at.c)
zephyr_library_sources(utils.c)
zephyr_library_sources(at-modem.c)

# zephyr_library_sources(utils.c)

add_subdirectory_ifdef(CONFIG_MODEM_SIM7600G simcom-sim7600g)
add_subdirectory_ifdef(CONFIG_MODEM_A7682E simcom-a7682e)
add_subdirectory_ifdef(CONFIG_MODEM_ESP32C3_AT esp32c3-at)

zephyr_include_directories(
  .
  ${ZEPHYR_BASE}/drivers/modem
)
