#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "Room.h"


LOG_MODULE_REGISTER(simple_server, LOG_LEVEL_DBG);
static uint16_t ui_port = 80;

static const uint8_t index_html[] = {
#include "index.html.gz.inc"
};

struct switch_command {
	int switch_num;
	int switch_val;
};

static const struct device *leds_dev = DEVICE_DT_GET_ANY(gpio_leds);

static const struct json_obj_descr switch_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct switch_command, switch_num, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct switch_command, switch_val, JSON_TOK_NUMBER),
};

static void parse_switch_post(uint8_t *buf, size_t len)
{
	int ret;
	struct switch_command cmd;
	const int expected_return_code = BIT_MASK(ARRAY_SIZE(switch_command_descr));

	ret = json_obj_parse(buf, len, switch_command_descr, ARRAY_SIZE(switch_command_descr), &cmd);
	k_free(buf);
	if (ret != expected_return_code) {
		LOG_WRN("Failed to fully parse JSON payload, ret=%d", ret);
		return;
	}

	LOG_INF("POST request setting Switch %d to state %d", cmd.switch_num, cmd.switch_val);

    const struct gpio_dt_spec *gpio = get_led_by_id(ROOM_LED_INFO);
	if (leds_dev != NULL) {
        gpio_pin_set(gpio->port, gpio->pin, cmd.switch_val);
	}
}

static int switch_handler(struct http_client_ctx *client, enum http_data_status status,
		       const struct http_request_ctx *request_ctx,
		       struct http_response_ctx *response_ctx, void *user_data)
{
	static uint16_t buffer_size = 128;
	uint8_t *post_payload_buf = k_malloc(buffer_size);
	static size_t cursor;

	LOG_DBG("Switch handler status %d, size %zu", status, request_ctx->data_len);

	if (status == HTTP_SERVER_DATA_ABORTED) {
		cursor = 0;
		k_free(post_payload_buf);
		return 0;
	}

	if (request_ctx->data_len + cursor > buffer_size) {
		cursor = 0;
		LOG_ERR("Size of the message is to long, please increase the buffer size\n");
		k_free(post_payload_buf);
		return -ENOMEM;
	}

	/* Copy payload to our buffer. Note that even for a small payload, it may arrive split into
	 * chunks (e.g. if the header size was such that the whole HTTP request exceeds the size of
	 * the client buffer).
	 */
	memcpy(post_payload_buf + cursor, request_ctx->data, request_ctx->data_len);
	cursor += request_ctx->data_len;

	if (status == HTTP_SERVER_DATA_FINAL) {
		parse_switch_post(post_payload_buf, cursor);
		cursor = 0;
	}

	return 0;
}

static struct http_resource_detail_static index_detail = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_STATIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET),
        .content_encoding = "gzip",
        .content_type = "text/html",
    },
    .static_data = index_html,
    .static_data_len = sizeof(index_html),
};

static struct http_resource_detail_dynamic switch_resource_detail = {
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		},
	.cb = switch_handler,
	.user_data = NULL,
};

HTTP_SERVICE_DEFINE(test_http_service, NULL, &ui_port, 1, 10, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(index_res, test_http_service, "/", &index_detail);

HTTP_RESOURCE_DEFINE(switch_res, test_http_service, "/led", &switch_resource_detail);
