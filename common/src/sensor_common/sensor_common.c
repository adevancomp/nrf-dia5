#include "sensor_common.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_common, CONFIG_SENSOR_COMMON_LOG_LEVEL);

void sensor_data_print(const sensor_data_t *data) {
    LOG_DBG("Sensor Data: Type: %d | Timestamp: %u", data->type, data->timestamp);
    LOG_HEXDUMP_DBG(data, sizeof(sensor_data_t), "Sensor Data:");

    switch (data->type) {
        case SENSOR_TYPE_TEMP:
            LOG_INF("Temperature: %.2f °C", (double)(data->values[0] / 100.0f));
            break;
        default:
            LOG_WRN("Unknown sensor type received.");
            break;
    }

    LOG_HEXDUMP_DBG(data, sizeof(sensor_data_t), "Sensor Data:");
}