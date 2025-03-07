#include "at-modem.h"

#include <string.h>

int atmodem_run_schedule(
    // struct modem_data *mdata,
    struct k_fifo *run_fifo,
    struct k_work_q *workq,
    struct k_work_delayable *run_worker,
    atmodem_work_handler_t handler,
    void *payload, size_t payload_size)
{
    struct fifo_item_t *payload_ptr = k_malloc(sizeof(struct fifo_item_t) + payload_size);
    __ASSERT(payload_ptr != 0, "Error alocation memory for FIFO payload");
    payload_ptr->handler = handler;
    payload_ptr->payload_size = payload_size;
    if(payload_size > 0) {
        memcpy(payload_ptr->payload, payload, payload_size);
    }
    k_fifo_put(run_fifo, payload_ptr);
    // k_work_submit_to_queue(&data->workq, &at_modem_fifo_work);
    k_work_schedule_for_queue(workq, run_worker, K_MSEC(CONFIG_ATMODEM_WORKER_DELAY));
    return 0;
}

int atmodem_execute(const struct device *dev, struct k_fifo *run_fifo, struct k_work_q *workq, struct k_work_delayable *run_worker)
{
    struct fifo_item_t *fifo_item = k_fifo_get(run_fifo, K_NO_WAIT);
    if(fifo_item) {
        //k_work_submit_to_queue(&data->workq, &at_modem_fifo_work);
        int ret = fifo_item->handler(fifo_item->payload, fifo_item->payload_size);

        k_free(fifo_item);
        k_work_schedule_for_queue(workq, run_worker, K_MSEC(CONFIG_ATMODEM_WORKER_DELAY));
        const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
        if(api->__handler_result_parser) {
            api->__handler_result_parser(dev, ret);
        }
    }
    return 0;
}

int atmodem_worker_schedule(struct k_work_q *workq, struct k_work_delayable *worker)
{
    return k_work_schedule_for_queue(workq, worker, K_MSEC(CONFIG_ATMODEM_WORKER_DELAY));
}
