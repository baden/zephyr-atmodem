#pragma once

#include <zephyr/sys_clock.h>
#include <stdint.h>
#include "../simcom-a7682e.h"

// Commands with simple wait answer
int simcom_cmd_with_simple_wait_answer(
    struct modem_data *mdata,
    const char *send_buf,
    const struct modem_cmd handler_cmds[],
    size_t handler_cmds_len,
    k_timeout_t timeout
);

// Command to send data to the modem after the ">" symbol
int simcom_cmd_with_direct_payload(struct modem_data *mdata,
    const char *send_buf,
    const char *data,
    size_t data_len,
    const struct modem_cmd *one_handler_cmds,
    k_timeout_t timeout
);

int simcom_at(const struct device *dev);
int simcom_cmd(const struct device *dev, const char *send_buf);
