// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dali/dali_api.h>
#include <dali/dali_ipc_protocol.h>

LOG_MODULE_REGISTER(dali_ipc, LOG_LEVEL_INF);

#define DALI_IPC_FRONTEND_TIMEOUT K_SECONDS(30)

static const struct device *const dali_ipc_instance =
	DEVICE_DT_GET(DT_NODELABEL(ipc0));
static struct ipc_ept dali_ipc_endpoint;
static K_SEM_DEFINE(dali_ipc_bound_sem, 0, 1);
static K_SEM_DEFINE(dali_ipc_response_sem, 0, 1);
static K_MUTEX_DEFINE(dali_ipc_lock);
static struct dali_ipc_message dali_ipc_response;
static uint8_t dali_ipc_next_sequence;
static bool dali_ipc_ready;

static void dali_ipc_ep_bound(void *priv)
{
	ARG_UNUSED(priv);

	k_sem_give(&dali_ipc_bound_sem);
}

static void dali_ipc_ep_received(const void *data, size_t len, void *priv)
{
	const struct dali_ipc_message *msg = data;

	ARG_UNUSED(priv);

	if ((data == NULL) || (len != sizeof(*msg))) {
		return;
	}

	if ((msg->version != DALI_IPC_PROTOCOL_VERSION) ||
	    (msg->type != DALI_IPC_MESSAGE_TYPE_RESPONSE)) {
		return;
	}

	memcpy(&dali_ipc_response, msg, sizeof(dali_ipc_response));
	k_sem_give(&dali_ipc_response_sem);
}

static struct ipc_ept_cfg dali_ipc_ep_cfg = {
	.name = "dali_api",
};

static int dali_ipc_frontend_ready(void)
{
	int err;

	if (dali_ipc_ready) {
		return 0;
	}

	if (!device_is_ready(dali_ipc_instance)) {
		return -ENODEV;
	}

	dali_ipc_ep_cfg.cb.bound = dali_ipc_ep_bound;
	dali_ipc_ep_cfg.cb.received = dali_ipc_ep_received;
	err = ipc_service_open_instance(dali_ipc_instance);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	err = ipc_service_register_endpoint(
		dali_ipc_instance, &dali_ipc_endpoint, &dali_ipc_ep_cfg);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	err = k_sem_take(&dali_ipc_bound_sem, DALI_IPC_FRONTEND_TIMEOUT);
	if (err != 0) {
		return err;
	}

	dali_ipc_ready = true;
	return 0;
}

static int dali_ipc_exchange(struct dali_ipc_message *request,
			     struct dali_ipc_message *response)
{
	int err;

	if ((request == NULL) || (response == NULL)) {
		return -EINVAL;
	}

	err = dali_ipc_frontend_ready();
	if (err != 0) {
		return err;
	}

	k_mutex_lock(&dali_ipc_lock, K_FOREVER);
	while (k_sem_take(&dali_ipc_response_sem, K_NO_WAIT) == 0) {
	}

	request->version = DALI_IPC_PROTOCOL_VERSION;
	request->type = DALI_IPC_MESSAGE_TYPE_REQUEST;
	request->sequence = ++dali_ipc_next_sequence;

	err = ipc_service_send(&dali_ipc_endpoint, request, sizeof(*request));
	if (err < 0) {
		k_mutex_unlock(&dali_ipc_lock);
		return err;
	}

	err = k_sem_take(&dali_ipc_response_sem, DALI_IPC_FRONTEND_TIMEOUT);
	if (err != 0) {
		k_mutex_unlock(&dali_ipc_lock);
		return err;
	}

	memcpy(response, &dali_ipc_response, sizeof(*response));
	k_mutex_unlock(&dali_ipc_lock);

	if ((response->version != DALI_IPC_PROTOCOL_VERSION) ||
	    (response->type != DALI_IPC_MESSAGE_TYPE_RESPONSE) ||
	    (response->sequence != request->sequence) ||
	    (response->opcode != request->opcode)) {
		return -EIO;
	}

	return response->rc;
}

int dali_api_init(void)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_INIT,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}

int dali_api_discover(struct dali_api_discovery_result *result)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_DISCOVER,
	};
	struct dali_ipc_message response;
	int err;

	if (result == NULL) {
		return -EINVAL;
	}

	memset(result, 0, sizeof(*result));
	err = dali_ipc_exchange(&request, &response);
	if (err == 0) {
		memcpy(result, &response.discovery, sizeof(*result));
	}

	return err;
}

int dali_api_off_short(uint8_t short_address)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_OFF_SHORT,
		.short_address = short_address,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}

int dali_api_recall_max_short(uint8_t short_address)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_RECALL_MAX_SHORT,
		.short_address = short_address,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}

int dali_api_set_level_short(uint8_t short_address, uint8_t level)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_SET_LEVEL_SHORT,
		.short_address = short_address,
		.value = level,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}

int dali_api_query_status_short(uint8_t short_address, uint8_t *status)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_QUERY_STATUS_SHORT,
		.short_address = short_address,
	};
	struct dali_ipc_message response;
	int err;

	if (status == NULL) {
		return -EINVAL;
	}

	*status = 0U;
	err = dali_ipc_exchange(&request, &response);
	if (err == 0) {
		*status = response.value;
	}

	return err;
}

int dali_api_query_actual_level_short(uint8_t short_address, uint8_t *level)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_QUERY_ACTUAL_LEVEL_SHORT,
		.short_address = short_address,
	};
	struct dali_ipc_message response;
	int err;

	if (level == NULL) {
		return -EINVAL;
	}

	*level = 0U;
	err = dali_ipc_exchange(&request, &response);
	if (err == 0) {
		*level = response.value;
	}

	return err;
}

int dali_api_off_broadcast(void)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_OFF_BROADCAST,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}

int dali_api_recall_max_broadcast(void)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_RECALL_MAX_BROADCAST,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}

int dali_api_set_level_broadcast(uint8_t level)
{
	struct dali_ipc_message request = {
		.opcode = DALI_IPC_OPCODE_SET_LEVEL_BROADCAST,
		.value = level,
	};
	struct dali_ipc_message response;

	return dali_ipc_exchange(&request, &response);
}
