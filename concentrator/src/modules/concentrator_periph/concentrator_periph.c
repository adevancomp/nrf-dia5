#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/scan.h>
#include "worker_shadow_service.h"

LOG_MODULE_REGISTER(concentrator_periph, CONFIG_CONCENTRATOR_PERIPH_LOG_LEVEL);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define FIXED_PASSKEY 123456

// BLE Advertising Data
const struct bt_data sensor_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),  // Add the No BR/EDR flag
    //Add WORKER_SHADOW_SERVICE_UUID to advertising data
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, WORKER_SHADOW_SERVICE_UUID),    
};

// Scan Response Data
const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),  // Add device name to advertising data
};


static void update_data_length(struct bt_conn *conn)
{
    int err;
    struct bt_conn_le_data_len_param my_data_len = {
        .tx_max_len = BT_GAP_DATA_LEN_MAX,
        .tx_max_time = BT_GAP_DATA_TIME_MAX,
    };
    err = bt_conn_le_data_len_update(conn, &my_data_len);
    if (err) {
        LOG_ERR("data_len_update failed (err %d)", err);
    }
}

static struct bt_gatt_exchange_params exchange_params;

static void exchange_func(struct bt_conn *conn, uint8_t att_err, struct bt_gatt_exchange_params *params);

static void update_mtu(struct bt_conn *conn)
{
    int err;
    exchange_params.func = exchange_func;

    err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        LOG_ERR("bt_gatt_exchange_mtu failed (err %d)", err);
    }
}

// Connection Callbacks
static void connected(struct bt_conn *conn, uint8_t err)
{
    struct bt_conn_info info; 
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_INF("Connection failed (err %u)\n", err);
        return;
    }
    else if(bt_conn_get_info(conn, &info)) {
        LOG_INF("Could not parse connection info\n");
    }
    else {
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        
        LOG_INF("Connection established to %s", addr);
    }   
    double connection_interval = info.le.interval*1.25; // in ms
    uint16_t supervision_timeout = info.le.timeout*10; // in ms
    LOG_INF("Connection parameters: interval %.2f ms, latency %d intervals, timeout %d ms",
                             connection_interval, info.le.latency, supervision_timeout);

    update_data_length(conn);
    update_mtu(conn);
                             

}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);
    // Additional disconnection handling code
    int err = bt_scan_stop();
    if (err) {
        LOG_ERR("Failed to stop scanning (err %d)", err);
        return;
    }
    bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, sensor_ad, ARRAY_SIZE(sensor_ad), sd, ARRAY_SIZE(sd));
    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Failed to start scanning (err %d)", err);
        return;
    }

}



void on_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
    double connection_interval = interval*1.25;         // in ms
    uint16_t supervision_timeout = timeout*10;          // in ms
    LOG_INF("Connection parameters updated: interval %.2f ms, latency %d intervals, timeout %d ms",
                        connection_interval, latency, supervision_timeout);
}

void on_le_data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info)
{
    uint16_t tx_len     = info->tx_max_len; 
    uint16_t tx_time    = info->tx_max_time;
    uint16_t rx_len     = info->rx_max_len;
    uint16_t rx_time    = info->rx_max_time;
    LOG_INF("Data length updated. Length %d/%d bytes, time %d/%d us", tx_len, rx_len, tx_time, rx_time);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
    .le_param_updated = on_le_param_updated,
    .le_data_len_updated    = on_le_data_len_updated,
    
};

static void exchange_func(struct bt_conn *conn, uint8_t att_err,
    struct bt_gatt_exchange_params *params)
{
    LOG_INF("MTU exchange %s", att_err == 0 ? "successful" : "failed");
    if (!att_err) {
        uint16_t payload_mtu = bt_gatt_get_mtu(conn) - 3;   // 3 bytes used for Attribute headers.
        LOG_INF("New MTU: %d bytes", payload_mtu);
    }
}

// BLE Ready Callback
void concentrator_periph_bt_ready(int err) {
    char addr_s[BT_ADDR_LE_STR_LEN];
    bt_addr_le_t addr = {0};
    size_t count = 1;

    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
        return;
    }
	
    bt_conn_cb_register(&conn_callbacks);

    LOG_INF("Bluetooth initialized");

    // Start connectable advertising
    bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, sensor_ad, ARRAY_SIZE(sensor_ad), sd, ARRAY_SIZE(sd));

    // Retrieve and log the Bluetooth address
    bt_id_get(&addr, &count);
    bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));
    LOG_INF("Bluetooth address: %s", addr_s);
}
// Initialize BLE
int concentrator_periph_init(void) {
    int err;

    bt_conn_cb_register(&conn_callbacks);
    
    return err; // Always return the error code from bt_enable to use on bt_ready // load_settings fix!
}
