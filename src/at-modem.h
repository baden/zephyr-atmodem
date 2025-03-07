#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "drivers/at-modem.h"

struct fifo_item_t {
    void* fifo_reserved;   /* 1st word reserved for use by FIFO */
    atmodem_work_handler_t handler;
    size_t payload_size;
    char payload[0];
};

int atmodem_run_schedule(
    // struct modem_data *mdata,
    struct k_fifo *run_fifo,
    struct k_work_q *workq,
    struct k_work_delayable *run_worker,
    atmodem_work_handler_t handler,
    void *payload, size_t payload_size);

// Must be called from modem workqueue
// TODO: Thing how we can exclude device from this function
int atmodem_execute(const struct device *dev, struct k_fifo *run_fifo, struct k_work_q *workq, struct k_work_delayable *run_worker);

int atmodem_worker_schedule(struct k_work_q *workq, struct k_work_delayable *worker);
