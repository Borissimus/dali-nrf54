// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dali/dali_api.h>

#include "cpuapp_heartbeat.h"

LOG_MODULE_REGISTER(dali_controller, LOG_LEVEL_INF);

#define DALI_CONTROLLER_LEVEL_MAX             0xFEU
#define DALI_CONTROLLER_LEVEL_STEP_20_PERCENT 51U
#define DALI_CONTROLLER_POLL_MS               50U
#define DALI_CONTROLLER_DEBOUNCE_MS           100U

#define BUTTON0_NODE DT_ALIAS(sw0)
#define BUTTON1_NODE DT_ALIAS(sw1)
#define BUTTON2_NODE DT_ALIAS(sw2)
#define BUTTON3_NODE DT_ALIAS(sw3)

#if !DT_NODE_HAS_STATUS_OKAY(BUTTON0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(BUTTON1_NODE)
#error "Unsupported board: sw1 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(BUTTON2_NODE)
#error "Unsupported board: sw2 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(BUTTON3_NODE)
#error "Unsupported board: sw3 devicetree alias is not defined"
#endif

struct dali_controller_button {
	const struct gpio_dt_spec *spec;
	bool sampled_pressed;
	bool stable_pressed;
	int64_t debounce_deadline_ms;
};

enum dali_controller_mode {
	DALI_CONTROLLER_MODE_BROADCAST = 0,
	DALI_CONTROLLER_MODE_ADDRESSED_GROUPS,
};

enum dali_controller_group {
	DALI_CONTROLLER_GROUP_EVEN = 0,
	DALI_CONTROLLER_GROUP_ODD,
	DALI_CONTROLLER_GROUP_COUNT,
};

static const struct gpio_dt_spec button0 =
	GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
static const struct gpio_dt_spec button1 =
	GPIO_DT_SPEC_GET(BUTTON1_NODE, gpios);
static const struct gpio_dt_spec button2 =
	GPIO_DT_SPEC_GET(BUTTON2_NODE, gpios);
static const struct gpio_dt_spec button3 =
	GPIO_DT_SPEC_GET(BUTTON3_NODE, gpios);

static struct dali_api_discovery_result dali_discovery;
static enum dali_controller_mode dali_controller_mode;
static uint8_t dali_broadcast_level = DALI_CONTROLLER_LEVEL_MAX;
static uint8_t dali_group_levels[DALI_CONTROLLER_GROUP_COUNT] = {
	DALI_CONTROLLER_LEVEL_MAX,
	DALI_CONTROLLER_LEVEL_MAX,
};

static int dali_controller_for_each_short(int (*fn)(uint8_t short_address))
{
	uint32_t i;

	for (i = 0; i < dali_discovery.count; i++) {
		int err = fn(dali_discovery.devices[i].short_address);

		if (err != 0) {
			return err;
		}
	}

	return 0;
}

static const char *dali_controller_group_name(enum dali_controller_group group)
{
	switch (group) {
	case DALI_CONTROLLER_GROUP_EVEN:
		return "even";
	case DALI_CONTROLLER_GROUP_ODD:
		return "odd";
	default:
		return "unknown";
	}
}

static bool dali_controller_short_in_group(uint8_t short_address,
					   enum dali_controller_group group)
{
	bool is_even = (short_address & 0x01U) == 0U;

	return (group == DALI_CONTROLLER_GROUP_EVEN) ? is_even : !is_even;
}

static uint32_t
dali_controller_group_device_count(enum dali_controller_group group)
{
	uint32_t count = 0U;
	uint32_t i;

	for (i = 0; i < dali_discovery.count; i++) {
		if (dali_controller_short_in_group(
			    dali_discovery.devices[i].short_address, group)) {
			count++;
		}
	}

	return count;
}

static int dali_controller_group_first_short(enum dali_controller_group group,
					     uint8_t *short_address)
{
	uint32_t i;

	for (i = 0; i < dali_discovery.count; i++) {
		uint8_t candidate = dali_discovery.devices[i].short_address;

		if (dali_controller_short_in_group(candidate, group)) {
			*short_address = candidate;
			return 0;
		}
	}

	return -ENOENT;
}

static int dali_controller_off_all(void)
{
	int err;

	if (dali_controller_mode == DALI_CONTROLLER_MODE_BROADCAST) {
		err = dali_api_off_broadcast();
		LOG_INF("off mode=broadcast rc=%d", err);
	} else {
		err = dali_controller_for_each_short(dali_api_off_short);
		LOG_INF("off mode=addressed count=%u rc=%d",
			dali_discovery.count, err);
	}

	if (err == 0) {
		dali_broadcast_level = 0U;
	}

	return err;
}

static int dali_controller_on_all(void)
{
	int err;

	if (dali_controller_mode == DALI_CONTROLLER_MODE_BROADCAST) {
		err = dali_api_recall_max_broadcast();
		LOG_INF("on mode=broadcast rc=%d", err);
	} else {
		err = dali_controller_for_each_short(dali_api_recall_max_short);
		LOG_INF("on mode=addressed count=%u rc=%d",
			dali_discovery.count, err);
	}

	if (err == 0) {
		dali_broadcast_level = DALI_CONTROLLER_LEVEL_MAX;
	}

	return err;
}

static int dali_controller_set_all_level(uint8_t level)
{
	int err;

	if (dali_controller_mode == DALI_CONTROLLER_MODE_BROADCAST) {
		err = dali_api_set_level_broadcast(level);
		LOG_INF("set_level mode=broadcast level=%u rc=%d", level, err);
	} else {
		uint32_t i;

		err = 0;
		for (i = 0; i < dali_discovery.count; i++) {
			err = dali_api_set_level_short(
				dali_discovery.devices[i].short_address, level);
			if (err != 0) {
				break;
			}
		}

		LOG_INF("set_level mode=addressed count=%u level=%u rc=%d",
			dali_discovery.count, level, err);
	}

	if (err == 0) {
		dali_broadcast_level = level;
	}

	return err;
}

static int dali_controller_group_set_level(enum dali_controller_group group,
					   uint8_t level)
{
	uint32_t count = 0U;
	uint32_t i;
	int err = 0;

	for (i = 0; i < dali_discovery.count; i++) {
		uint8_t short_address = dali_discovery.devices[i].short_address;

		if (!dali_controller_short_in_group(short_address, group)) {
			continue;
		}

		err = dali_api_set_level_short(short_address, level);
		if (err != 0) {
			break;
		}

		count++;
	}

	if (count == 0U) {
		LOG_INF("set_level group=%s empty, skipping",
			dali_controller_group_name(group));
		return 0;
	}

	if (err == 0) {
		dali_group_levels[group] = level;
	}

	LOG_INF("set_level group=%s count=%u level=%u rc=%d",
		dali_controller_group_name(group), count, level, err);
	return err;
}

static uint8_t dali_controller_level_add_wrap(uint8_t level)
{
	uint16_t next = (uint16_t)level + DALI_CONTROLLER_LEVEL_STEP_20_PERCENT;

	if (level >= DALI_CONTROLLER_LEVEL_MAX) {
		return 0U;
	}

	if (next > DALI_CONTROLLER_LEVEL_MAX) {
		return DALI_CONTROLLER_LEVEL_MAX;
	}

	return (uint8_t)next;
}

static uint8_t dali_controller_level_down(uint8_t level)
{
	if (level <= DALI_CONTROLLER_LEVEL_STEP_20_PERCENT) {
		return 0U;
	}

	return level - DALI_CONTROLLER_LEVEL_STEP_20_PERCENT;
}

static uint8_t dali_controller_level_up(uint8_t level)
{
	uint16_t next = (uint16_t)level + DALI_CONTROLLER_LEVEL_STEP_20_PERCENT;

	if (next > DALI_CONTROLLER_LEVEL_MAX) {
		return DALI_CONTROLLER_LEVEL_MAX;
	}

	return (uint8_t)next;
}

static int dali_controller_toggle_group(enum dali_controller_group group)
{
	uint8_t next_level;
	int err;

	next_level = (dali_group_levels[group] > 0U)
			     ? 0U
			     : DALI_CONTROLLER_LEVEL_MAX;
	err = dali_controller_group_set_level(group, next_level);
	LOG_INF("toggle group=%s level=%u rc=%d",
		dali_controller_group_name(group), next_level, err);
	return err;
}

static int dali_controller_step_group(enum dali_controller_group group)
{
	uint8_t next_level =
		dali_controller_level_add_wrap(dali_group_levels[group]);
	int err = dali_controller_group_set_level(group, next_level);

	LOG_INF("step group=%s level=%u rc=%d",
		dali_controller_group_name(group), next_level, err);
	return err;
}

static int dali_controller_configure_button(const struct gpio_dt_spec *button)
{
	int err;

	if (!gpio_is_ready_dt(button)) {
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(button, GPIO_INPUT);
	if (err != 0) {
		return err;
	}

	return 0;
}

static bool dali_controller_button_pressed(const struct gpio_dt_spec *button)
{
	int value = gpio_pin_get_dt(button);

	return value > 0;
}

static int dali_controller_handle_button_action(size_t button_index)
{
	if (dali_controller_mode == DALI_CONTROLLER_MODE_ADDRESSED_GROUPS) {
		switch (button_index) {
		case 0:
			return dali_controller_toggle_group(
				DALI_CONTROLLER_GROUP_EVEN);
		case 1:
			return dali_controller_toggle_group(
				DALI_CONTROLLER_GROUP_ODD);
		case 2:
			return dali_controller_step_group(
				DALI_CONTROLLER_GROUP_EVEN);
		case 3:
			return dali_controller_step_group(
				DALI_CONTROLLER_GROUP_ODD);
		default:
			return -EINVAL;
		}
	}

	switch (button_index) {
	case 0:
		return dali_controller_off_all();
	case 1:
		return dali_controller_on_all();
	case 2:
		return dali_controller_set_all_level(
			dali_controller_level_down(dali_broadcast_level));
	case 3:
		return dali_controller_set_all_level(
			dali_controller_level_up(dali_broadcast_level));
	default:
		return -EINVAL;
	}
}

static void
dali_controller_handle_buttons(struct dali_controller_button *buttons,
			       size_t count)
{
	int64_t now_ms = k_uptime_get();
	size_t i;

	for (i = 0; i < count; i++) {
		bool pressed = dali_controller_button_pressed(buttons[i].spec);

		if (pressed != buttons[i].sampled_pressed) {
			buttons[i].sampled_pressed = pressed;
			buttons[i].debounce_deadline_ms =
				now_ms + DALI_CONTROLLER_DEBOUNCE_MS;
			continue;
		}

		if (buttons[i].stable_pressed == buttons[i].sampled_pressed) {
			continue;
		}

		if (now_ms < buttons[i].debounce_deadline_ms) {
			continue;
		}

		buttons[i].stable_pressed = buttons[i].sampled_pressed;
		if (buttons[i].stable_pressed) {
			int err = dali_controller_handle_button_action(i);

			if (err != 0) {
				LOG_ERR("button%u action failed -> %d",
					(unsigned int)i, err);
			}
		}
	}
}

int main(void)
{
	int err;
	struct dali_controller_button buttons[] = {
		{.spec = &button0},
		{.spec = &button1},
		{.spec = &button2},
		{.spec = &button3},
	};
	size_t i;

	LOG_INF("starting CPUAPP DALI controller");

	err = cpuapp_heartbeat_init();
	if (err != 0) {
		LOG_ERR("cpuapp_heartbeat_init failed -> %d", err);
		return err;
	}

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		err = dali_controller_configure_button(buttons[i].spec);
		if (err != 0) {
			LOG_ERR("button%u init failed -> %d", (unsigned int)i,
				err);
			return err;
		}
	}

	err = dali_api_init();
	if (err != 0) {
		LOG_ERR("dali_api_init failed -> %d", err);
		return err;
	}

	k_sleep(K_SECONDS(1));

	err = dali_api_discover(&dali_discovery);
	if (err == 0 && dali_discovery.count > 0U) {
		enum dali_controller_group group;

		dali_controller_mode = DALI_CONTROLLER_MODE_ADDRESSED_GROUPS;
		LOG_INF("discovered %u devices", dali_discovery.count);
		LOG_INF("%s", "addressed even/odd groups");

		for (group = DALI_CONTROLLER_GROUP_EVEN;
		     group < DALI_CONTROLLER_GROUP_COUNT; group++) {
			uint8_t short_address;
			uint8_t level = DALI_CONTROLLER_LEVEL_MAX;
			uint32_t count =
				dali_controller_group_device_count(group);

			if (count == 0U) {
				dali_group_levels[group] =
					DALI_CONTROLLER_LEVEL_MAX;
				LOG_INF("group=%s has no devices",
					dali_controller_group_name(group));
				continue;
			}

			err = dali_controller_group_first_short(group,
								&short_address);
			if (err != 0) {
				dali_group_levels[group] =
					DALI_CONTROLLER_LEVEL_MAX;
				LOG_WRN("group=%s first lookup rc=%d",
					dali_controller_group_name(group), err);
				continue;
			}

			err = dali_api_query_actual_level_short(short_address,
								&level);
			if (err == 0) {
				dali_group_levels[group] = level;
			} else {
				dali_group_levels[group] =
					DALI_CONTROLLER_LEVEL_MAX;
				LOG_WRN("group=%s level query rc=%d, using max",
					dali_controller_group_name(group), err);
			}

			LOG_INF("group=%s count=%u short=0x%02x level=%u",
				dali_controller_group_name(group), count,
				short_address, dali_group_levels[group]);
		}
	} else {
		dali_controller_mode = DALI_CONTROLLER_MODE_BROADCAST;
		dali_discovery.count = 0U;
		dali_broadcast_level = DALI_CONTROLLER_LEVEL_MAX;
		if (err != 0) {
			LOG_WRN("broadcast fallback rc=%d", err);
		} else {
			LOG_WRN("%s", "no devices, broadcast fallback");
		}
	}

	while (true) {
		dali_controller_handle_buttons(buttons, ARRAY_SIZE(buttons));
		k_sleep(K_MSEC(DALI_CONTROLLER_POLL_MS));
	}
}
