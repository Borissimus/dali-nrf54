// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <dali/dali_api.h>
#include <dali/dali_ipc_protocol.h>

LOG_MODULE_REGISTER(dali_ipc_server, CONFIG_DALI_FLPR_LOG_LEVEL);

static const struct device *const dali_ipc_instance =
	DEVICE_DT_GET(DT_NODELABEL(ipc0));
static struct ipc_ept dali_ipc_endpoint;
static K_SEM_DEFINE(dali_ipc_bound_sem, 0, 1);
static struct k_work dali_ipc_request_work;
static struct dali_ipc_message dali_ipc_pending_request;
static bool dali_ipc_server_ready;
static bool dali_ipc_request_valid;

static void dali_ipc_server_bound(void *priv)
{
	ARG_UNUSED(priv);

	k_sem_give(&dali_ipc_bound_sem);
}

static int
dali_ipc_server_send_response(const struct dali_ipc_message *response)
{
	if (response == NULL) {
		return -EINVAL;
	}

	return ipc_service_send(&dali_ipc_endpoint, response,
				sizeof(*response));
}

static void dali_ipc_server_process(struct k_work *work)
{
	struct dali_ipc_message request;
	struct dali_ipc_message response;
	int err = 0;

	ARG_UNUSED(work);

	if (!dali_ipc_request_valid) {
		return;
	}

	memcpy(&request, &dali_ipc_pending_request, sizeof(request));
	dali_ipc_request_valid = false;

	memset(&response, 0, sizeof(response));
	response.version = DALI_IPC_PROTOCOL_VERSION;
	response.type = DALI_IPC_MESSAGE_TYPE_RESPONSE;
	response.opcode = request.opcode;
	response.sequence = request.sequence;

	switch (request.opcode) {
	case DALI_IPC_OPCODE_INIT:
		err = dali_api_init();
		break;
	case DALI_IPC_OPCODE_DISCOVER:
		err = dali_api_discover(&response.discovery);
		break;
	case DALI_IPC_OPCODE_OFF_SHORT:
		err = dali_api_off_short(request.short_address);
		break;
	case DALI_IPC_OPCODE_RECALL_MAX_SHORT:
		err = dali_api_recall_max_short(request.short_address);
		break;
	case DALI_IPC_OPCODE_SET_LEVEL_SHORT:
		err = dali_api_set_level_short(request.short_address,
					       request.value);
		break;
	case DALI_IPC_OPCODE_QUERY_STATUS_SHORT:
		err = dali_api_query_status_short(request.short_address,
						  &response.value);
		break;
	case DALI_IPC_OPCODE_QUERY_ACTUAL_LEVEL_SHORT:
		err = dali_api_query_actual_level_short(request.short_address,
							&response.value);
		break;
	case DALI_IPC_OPCODE_OFF_BROADCAST:
		err = dali_api_off_broadcast();
		break;
	case DALI_IPC_OPCODE_RECALL_MAX_BROADCAST:
		err = dali_api_recall_max_broadcast();
		break;
	case DALI_IPC_OPCODE_SET_LEVEL_BROADCAST:
		err = dali_api_set_level_broadcast(request.value);
		break;
	default:
		err = -EINVAL;
		break;
	}

	response.rc = err;
	err = dali_ipc_server_send_response(&response);
	if (err < 0) {
		LOG_ERR("failed to send response -> %d", err);
	}
}

static void dali_ipc_server_received(const void *data, size_t len, void *priv)
{
	const struct dali_ipc_message *msg = data;
	struct dali_ipc_message response;

	ARG_UNUSED(priv);

	if ((data == NULL) || (len != sizeof(*msg))) {
		return;
	}

	if ((msg->version != DALI_IPC_PROTOCOL_VERSION) ||
	    (msg->type != DALI_IPC_MESSAGE_TYPE_REQUEST)) {
		return;
	}

	if (dali_ipc_request_valid) {
		memset(&response, 0, sizeof(response));
		response.version = DALI_IPC_PROTOCOL_VERSION;
		response.type = DALI_IPC_MESSAGE_TYPE_RESPONSE;
		response.opcode = msg->opcode;
		response.sequence = msg->sequence;
		response.rc = -EBUSY;
		(void)dali_ipc_server_send_response(&response);
		return;
	}

	memcpy(&dali_ipc_pending_request, msg,
	       sizeof(dali_ipc_pending_request));
	dali_ipc_request_valid = true;
	k_work_submit(&dali_ipc_request_work);
}

static struct ipc_ept_cfg dali_ipc_ep_cfg = {
	.name = "dali_api",
};

int dali_ipc_server_init(void)
{
	int err;

	if (dali_ipc_server_ready) {
		return 0;
	}

	if (!device_is_ready(dali_ipc_instance)) {
		return -ENODEV;
	}

	dali_ipc_ep_cfg.cb.bound = dali_ipc_server_bound;
	dali_ipc_ep_cfg.cb.received = dali_ipc_server_received;
	k_work_init(&dali_ipc_request_work, dali_ipc_server_process);

	err = ipc_service_open_instance(dali_ipc_instance);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	err = ipc_service_register_endpoint(
		dali_ipc_instance, &dali_ipc_endpoint, &dali_ipc_ep_cfg);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	err = k_sem_take(&dali_ipc_bound_sem, K_SECONDS(5));
	if (err != 0) {
		return err;
	}

	dali_ipc_server_ready = true;
	LOG_INF("IPC server ready");
	return 0;
}
