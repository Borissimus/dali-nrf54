/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: Copyright (c) 2026 N-iX */

#ifndef DALI_CPU_LOAD_REPORTER_H_
#define DALI_CPU_LOAD_REPORTER_H_

void dali_cpu_load_reporter_start(void);
void dali_cpu_load_reporter_reset(void);
void dali_cpu_load_reporter_report(const char *window);

#endif /* DALI_CPU_LOAD_REPORTER_H_ */
