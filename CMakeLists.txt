# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)
zephyr_include_directories(..) # Шо за нах?
add_subdirectory_ifdef(CONFIG_ATMODEM_DRV src)

