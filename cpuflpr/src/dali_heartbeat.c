// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdbool.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dali_heartbeat, CONFIG_DALI_FLPR_LOG_LEVEL);

#define DALI_HEARTBEAT_MS   500U
#define DALI_HEARTBEAT_NODE DT_ALIAS(led1)

#if !DT_NODE_HAS_STATUS_OKAY(DALI_HEARTBEAT_NODE)
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec dali_heartbeat_led =
	GPIO_DT_SPEC_GET(DALI_HEARTBEAT_NODE, gpios);

K_THREAD_STACK_DEFINE(dali_heartbeat_stack, 512);
static struct k_thread dali_heartbeat_thread;
static bool dali_heartbeat_started;

static void dali_heartbeat_thread_fn(void *arg1, void *arg2, void *arg3)
{
	int err;
	bool state = false;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		state = !state;
		err = gpio_pin_set_dt(&dali_heartbeat_led, state);
		if (err != 0) {
			LOG_ERR("LED1 set failed -> %d", err);
			return;
		}

		k_sleep(K_MSEC(DALI_HEARTBEAT_MS));
	}
}

int dali_heartbeat_init(void)
{
	int err;

	if (dali_heartbeat_started) {
		return 0;
	}

	if (!gpio_is_ready_dt(&dali_heartbeat_led)) {
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&dali_heartbeat_led, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		return err;
	}

	(void)k_thread_create(&dali_heartbeat_thread, dali_heartbeat_stack,
			      K_THREAD_STACK_SIZEOF(dali_heartbeat_stack),
			      dali_heartbeat_thread_fn, NULL, NULL, NULL, 7, 0,
			      K_NO_WAIT);

	dali_heartbeat_started = true;
	return 0;
}
