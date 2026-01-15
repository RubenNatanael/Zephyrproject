#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "Room.h"


LOG_MODULE_REGISTER(web_server, LOG_LEVEL_DBG);
static uint16_t ui_port = 80;

static const uint8_t index_html[] = {
#include "index.html.gz.inc"
};

struct led_command {
	int led_num;
	int led_val;
};

struct room_light_command {
	int room_id;
	int light_value;
};

static const struct json_obj_descr led_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct led_command, led_num, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct led_command, led_val, JSON_TOK_NUMBER),
};

static const struct json_obj_descr room_light_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct room_light_command, room_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct room_light_command, light_value, JSON_TOK_NUMBER),
};

typedef void (*post_parser_fn)(uint8_t *buf, size_t len);

struct post_state {
	uint8_t *buf;
	size_t cursor;
	uint16_t max_size;
	post_parser_fn parser;
};

static void parse_led_post(uint8_t *buf, size_t len)
{
	int ret;
	struct led_command cmd;
	const int expected_return_code = BIT_MASK(ARRAY_SIZE(led_command_descr));

	buf[len] = '\0';
	ret = json_obj_parse(buf, len, led_command_descr, ARRAY_SIZE(led_command_descr), &cmd);
	if (ret != expected_return_code) {
		LOG_WRN("Failed to fully parse JSON payload, ret=%d", ret);
		return;
	}

	LOG_INF("POST request setting LED %d to state %d", cmd.led_num, cmd.led_val);

    const struct gpio_dt_spec *gpio = get_led_by_id(ROOM_LED_INFO);

    gpio_pin_set(gpio->port, gpio->pin, cmd.led_val);
}

static void parse_room_light_post(uint8_t *buf, size_t len)
{
	int ret;
	struct room_light_command cmd;
	const int expected_return_code = BIT_MASK(ARRAY_SIZE(room_light_command_descr));

	buf[len] = '\0';
	ret = json_obj_parse(buf, len, room_light_command_descr, ARRAY_SIZE(room_light_command_descr), &cmd);
	if (ret != expected_return_code) {
		LOG_WRN("Failed to fully parse JSON payload, ret=%d", ret);
		return;
	}

	LOG_INF("POST request setting LED %d to state %d", cmd.room_id, cmd.light_value);

    const struct Room *room = get_room_by_id(cmd.room_id);
	if (room != NULL) {

        struct Event *new_event = k_malloc(sizeof(struct Event));
		if (!new_event) {
			LOG_ERR("Unable to allocate memory for event");
			return;
		}

		/* Register light events */
		if (room->light_gpio != NULL) {
			new_event->action = gpio_event_action;
			new_event->ctx = (void *)room->light_gpio;
			new_event->value = cmd.light_value;
		}
		if (room->light_pwm != NULL) {
			new_event->action = pwm_event_action;
			new_event->ctx = (void *)room->light_pwm;
			new_event->value = cmd.light_value;;
		}
		k_fifo_put(&events_fifo, new_event);
	}
}

static struct post_state led_post_state = {
	.buf = NULL,
	.cursor = 0,
	.max_size = 64,
	.parser = parse_led_post,
};

static struct post_state room_light_post_state = {
	.buf = NULL,
	.cursor = 0,
	.max_size = 64,
	.parser = parse_room_light_post,
};


static int post_handler(struct http_client_ctx *client, enum http_data_status status,
		       const struct http_request_ctx *request_ctx,
		       struct http_response_ctx *response_ctx, void *user_data)
{

	struct post_state *state = user_data;

	LOG_DBG("POST handler status %d, size %zu", status, request_ctx->data_len);

	if (status == HTTP_SERVER_DATA_ABORTED) {
		state->cursor = 0;
		return 0;
	}

	if (state->buf == NULL) {
        state->buf = (uint8_t *)k_malloc(state->max_size);
        if (!state->buf) {
            LOG_ERR("Out of memory for POST buffer");
            return -ENOMEM;
        }
        state->cursor = 0;
    }

	if (request_ctx->data_len + state->cursor > state->max_size) {
		state->cursor = 0;
		LOG_ERR("Size of the message is to long, please increase the buffer size");
		k_free(state->buf);
		return -ENOMEM;
	}

	/* Copy payload to our buffer. Note that even for a small payload, it may arrive split into
	 * chunks (e.g. if the header size was such that the whole HTTP request exceeds the size of
	 * the client buffer).
	 */
	memcpy(state->buf + state->cursor, request_ctx->data, request_ctx->data_len);
	state->cursor += request_ctx->data_len;

	if (status == HTTP_SERVER_DATA_FINAL) {
		state->parser(state->buf, state->cursor);
		k_free(state->buf);
		state->buf = NULL;
		state->cursor = 0;
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

static struct http_resource_detail_dynamic led_resource_detail = {
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		},
	.cb = post_handler,
	.user_data = &led_post_state,
};

static struct http_resource_detail_dynamic room_light_resource_detail = {
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		},
	.cb = post_handler,
	.user_data = &room_light_post_state,
};

HTTP_SERVICE_DEFINE(test_http_service, NULL, &ui_port, 1, 10, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(index_res, test_http_service, "/", &index_detail);

HTTP_RESOURCE_DEFINE(led_res, test_http_service, "/api/v1/led", &led_resource_detail);

HTTP_RESOURCE_DEFINE(light_res, test_http_service, "/api/v1/light", &room_light_resource_detail);
