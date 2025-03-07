zephyr_library()

if(CONFIG_MODEM_ESP32C3_AT)
    zephyr_library_sources(at.c)
    zephyr_library_sources(id.c)
    zephyr_library_sources(server.c)
    zephyr_library_sources_ifdef(CONFIG_MODEM_SIM7600G rsa.c)
endif()

# zephyr_library_include_directories(
#   ${ZEPHYR_BASE}/drivers/modem
# )
