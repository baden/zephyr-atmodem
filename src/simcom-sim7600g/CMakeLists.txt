zephyr_library()

zephyr_library_sources(onoff.c)
zephyr_library_sources(id.c)
zephyr_library_sources(gprs.c)
zephyr_library_sources(http.c)
zephyr_library_sources(sms.c)
zephyr_library_sources(tcp.c)
zephyr_library_sources(dns.c)
zephyr_library_sources(tests.c)
zephyr_library_sources(handlers.c)
# zephyr_library_sources_ifdef(CONFIG_MODEM_SIM7600G crc8.c)

# zephyr_include_directories_ifdef(CONFIG_MODEM_SIM7600G .)

# zephyr_library_include_directories(
#   ${ZEPHYR_BASE}/drivers/modem
# )
