# Драйвер AT-модему для Zephyr OS

## Підтримувані пристрої

- SIMCOM A7682E

## Планується підтримка:

- SIMCOM SIM7600G
- ESP32C3 з прошивкою ESP-AT


TODO:
- Для SIMCOM A7682E:
  - перехопити всі:
    - [ ] modem_cmd_send
    - [ ] modem_cmd_send_ext
    - [ ] modem_cmd_send_nolock (?)

  dns: int simcom_dns(struct modem_data *mdata, const char* domain_name, char* resolved_name)
  mqtt: simcom_mqtt_connected(const struct device *dev, bool *connected)

  