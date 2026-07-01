/* SPDX-License-Identifier: Apache-2.0 */

#ifndef DALI_API_H_
#define DALI_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DALI_API_MAX_DEVICES 64U

struct dali_api_device {
	uint8_t short_address;
};

struct dali_api_discovery_result {
	uint8_t count;
	uint8_t truncated;
	struct dali_api_device devices[DALI_API_MAX_DEVICES];
};

int dali_api_init(void);
int dali_api_discover(struct dali_api_discovery_result *result);
int dali_api_off_short(uint8_t short_address);
int dali_api_recall_max_short(uint8_t short_address);
int dali_api_set_level_short(uint8_t short_address, uint8_t level);
int dali_api_query_status_short(uint8_t short_address, uint8_t *status);
int dali_api_query_actual_level_short(uint8_t short_address, uint8_t *level);
int dali_api_off_broadcast(void);
int dali_api_recall_max_broadcast(void);
int dali_api_set_level_broadcast(uint8_t level);

#ifdef __cplusplus
}
#endif

#endif /* DALI_API_H_ */
