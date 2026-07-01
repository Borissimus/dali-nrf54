// SPDX-License-Identifier: Apache-2.0

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dali/dali_api.h>

#include "dali_services_flpr.h"

LOG_MODULE_REGISTER(dali_flpr_main, CONFIG_DALI_FLPR_LOG_LEVEL);

int main(void)
{
	int err;

	LOG_INF("CPUFLPR runtime started");

	err = dali_heartbeat_init();
	if (err != 0) {
		LOG_ERR("heartbeat init failed -> %d", err);
		return err;
	}

	err = dali_api_init();
	if (err != 0) {
		LOG_ERR("dali_api_init failed -> %d", err);
		return err;
	}

	err = dali_ipc_server_init();
	if (err != 0) {
		LOG_ERR("ipc server init failed -> %d", err);
		return err;
	}

	LOG_INF("IPC runtime ready");
	while (true) {
		k_sleep(K_FOREVER);
	}
}
