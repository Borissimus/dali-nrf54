// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 N-iX

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_load.h"

LOG_MODULE_REGISTER(dali_ble_load, LOG_LEVEL_INF);

#define DALI_BLE_LOAD_ADV_INTERVAL 0x0020U

static const struct bt_le_adv_param dali_ble_load_adv_param =
	BT_LE_ADV_PARAM_INIT(0, DALI_BLE_LOAD_ADV_INTERVAL,
			     DALI_BLE_LOAD_ADV_INTERVAL, NULL);
static const struct bt_data dali_ble_load_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1U),
};

int dali_ble_load_start(void)
{
	int err;

	err = bt_enable(NULL);
	if (err != 0) {
		return err;
	}

	err = bt_le_adv_start(&dali_ble_load_adv_param, dali_ble_load_ad,
			      ARRAY_SIZE(dali_ble_load_ad), NULL, 0);
	if (err == 0) {
		LOG_INF("BLE load advertising interval=20ms");
	}

	return err;
}
