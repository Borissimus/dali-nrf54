// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdbool.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "cpuapp_heartbeat.h"

LOG_MODULE_REGISTER(cpuapp_heartbeat, LOG_LEVEL_INF);

#define CPUAPP_HEARTBEAT_NODE DT_ALIAS(led0)
#define CPUAPP_HEARTBEAT_MS   500U

#if !DT_NODE_HAS_STATUS_OKAY(CPUAPP_HEARTBEAT_NODE)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec cpuapp_heartbeat_led =
	GPIO_DT_SPEC_GET(CPUAPP_HEARTBEAT_NODE, gpios);

K_THREAD_STACK_DEFINE(cpuapp_heartbeat_stack, 512);
static struct k_thread cpuapp_heartbeat_thread;
static bool cpuapp_heartbeat_started;

static void cpuapp_heartbeat_thread_fn(void *arg1, void *arg2, void *arg3)
{
	int err;
	bool state = false;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		state = !state;
		err = gpio_pin_set_dt(&cpuapp_heartbeat_led, state);
		if (err != 0) {
			LOG_ERR("LED0 set failed -> %d", err);
			return;
		}

		k_sleep(K_MSEC(CPUAPP_HEARTBEAT_MS));
	}
}

int cpuapp_heartbeat_init(void)
{
	int err;

	if (cpuapp_heartbeat_started) {
		return 0;
	}

	if (!gpio_is_ready_dt(&cpuapp_heartbeat_led)) {
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&cpuapp_heartbeat_led,
				    GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		return err;
	}

	(void)k_thread_create(&cpuapp_heartbeat_thread, cpuapp_heartbeat_stack,
			      K_THREAD_STACK_SIZEOF(cpuapp_heartbeat_stack),
			      cpuapp_heartbeat_thread_fn, NULL, NULL, NULL, 7,
			      0, K_NO_WAIT);

	cpuapp_heartbeat_started = true;
	return 0;
}
