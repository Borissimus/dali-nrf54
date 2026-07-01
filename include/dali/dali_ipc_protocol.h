/* SPDX-License-Identifier: Apache-2.0 */

#ifndef DALI_IPC_PROTOCOL_H_
#define DALI_IPC_PROTOCOL_H_

#include <stdint.h>

#include <dali/dali_api.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DALI_IPC_PROTOCOL_VERSION 1U

enum dali_ipc_message_type {
	DALI_IPC_MESSAGE_TYPE_REQUEST = 1U,
	DALI_IPC_MESSAGE_TYPE_RESPONSE = 2U,
};

enum dali_ipc_opcode {
	DALI_IPC_OPCODE_INIT = 1U,
	DALI_IPC_OPCODE_DISCOVER = 2U,
	DALI_IPC_OPCODE_OFF_SHORT = 3U,
	DALI_IPC_OPCODE_RECALL_MAX_SHORT = 4U,
	DALI_IPC_OPCODE_SET_LEVEL_SHORT = 5U,
	DALI_IPC_OPCODE_QUERY_STATUS_SHORT = 6U,
	DALI_IPC_OPCODE_QUERY_ACTUAL_LEVEL_SHORT = 7U,
	DALI_IPC_OPCODE_OFF_BROADCAST = 8U,
	DALI_IPC_OPCODE_RECALL_MAX_BROADCAST = 9U,
	DALI_IPC_OPCODE_SET_LEVEL_BROADCAST = 10U,
};

struct dali_ipc_message {
	uint8_t version;
	uint8_t type;
	uint8_t opcode;
	uint8_t sequence;
	int32_t rc;
	uint8_t short_address;
	uint8_t value;
	uint8_t reserved0;
	uint8_t reserved1;
	struct dali_api_discovery_result discovery;
};

#ifdef __cplusplus
}
#endif

#endif /* DALI_IPC_PROTOCOL_H_ */
