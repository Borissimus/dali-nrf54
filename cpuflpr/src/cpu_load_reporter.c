// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 N-iX

#include <zephyr/debug/cpu_load.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "cpu_load_reporter.h"

LOG_MODULE_REGISTER(dali_flpr_cpu_load, CONFIG_DALI_FLPR_LOG_LEVEL);

static void dali_cpu_load_report(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(dali_cpu_load_work, dali_cpu_load_report);

void dali_cpu_load_reporter_reset(void)
{
	(void)cpu_load_get(true);
}

void dali_cpu_load_reporter_report(const char *window)
{
	int load_per_mille = cpu_load_get(true);

	if (load_per_mille < 0) {
		LOG_ERR("core=cpuflpr load read failed rc=%d", load_per_mille);
	} else {
		LOG_INF("core=cpuflpr load=%d.%d%% window=%s",
			load_per_mille / 10, load_per_mille % 10, window);
	}
}

static void dali_cpu_load_report(struct k_work *work)
{
	ARG_UNUSED(work);

	dali_cpu_load_reporter_report("periodic");

	(void)k_work_reschedule(
		&dali_cpu_load_work,
		K_SECONDS(CONFIG_DALI_FLPR_CPU_LOAD_REPORT_INTERVAL_S));
}

void dali_cpu_load_reporter_start(void)
{
	dali_cpu_load_reporter_reset();
	(void)k_work_reschedule(
		&dali_cpu_load_work,
		K_SECONDS(CONFIG_DALI_FLPR_CPU_LOAD_REPORT_INTERVAL_S));
}
