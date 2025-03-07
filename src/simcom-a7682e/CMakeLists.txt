zephyr_library()

zephyr_library_sources(onoff.c)
zephyr_library_sources(id.c)
zephyr_library_sources(gprs.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_HTTP http.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_SMS sms.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_TCP tcp.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_DNS dns.c)
zephyr_library_sources(tests.c)
zephyr_library_sources(ssl.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_MQTT mqtt.c)
zephyr_library_sources(handlers.c)
zephyr_library_sources(common.c)
zephyr_library_sources_ifdef(CONFIG_MODEM_VOICE voice.c)
zephyr_library_sources(fs.c)
zephyr_library_sources(ussd.c)
# zephyr_library_sources_ifdef(CONFIG_MODEM_SIM7600G crc8.c)

# zephyr_include_directories_ifdef(CONFIG_MODEM_SIM7600G .)

# zephyr_library_include_directories(
#   ${ZEPHYR_BASE}/drivers/modem
# )
