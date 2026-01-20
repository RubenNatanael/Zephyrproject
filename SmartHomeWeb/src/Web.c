#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/websocket.h>
#include <zephyr/sys/time_units.h>

#include "Room.h"

#define MAX_ROOMS 5

LOG_MODULE_REGISTER(web_server, LOG_LEVEL_DBG);
static uint16_t ui_port = 80;

static const uint8_t index_html[] = {
#include "index.html.gz.inc"
};

/* JSON commands definition */
struct led_command {
	int led_num;
	bool led_val;
};
static const struct json_obj_descr led_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct led_command, led_num, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct led_command, led_val, JSON_TOK_TRUE),
};

struct room_light_command {
	int room_id;
	int light_value;
};
static const struct json_obj_descr room_light_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct room_light_command, room_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct room_light_command, light_value, JSON_TOK_NUMBER),
};


// JSON commands for temperature and humidity only for reading
struct room_temp_read_command {
	int room_id;
	int temp_value;
	int hum_value;
};

static const struct json_obj_descr room_temp_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct room_temp_read_command, room_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct room_temp_read_command, temp_value, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct room_temp_read_command, hum_value, JSON_TOK_NUMBER),
};

// JSON commands for temperature setting
struct room_temp_set_command {
	int room_id;
	int desire_temp_value;
};
static const struct json_obj_descr room_temp_set_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct room_temp_set_command, room_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct room_temp_set_command, desire_temp_value, JSON_TOK_NUMBER),
};
struct RoomData {
    uint32_t room_id;
    const char* room_name;
    uint32_t temp_sensor_value;
    uint32_t hum_sensor_value;
    uint16_t light_gpio_value;
    uint32_t desired_temperature;
    bool heat_relay_state;
};

struct RoomCollection {
    struct RoomData rooms[STRUCT_ROOM_COUNT];
    size_t num_rooms;
};

static const struct json_obj_descr room_command_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct RoomData, room_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct RoomData, room_name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct RoomData, temp_sensor_value, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct RoomData, hum_sensor_value, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct RoomData, light_gpio_value, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct RoomData, desired_temperature, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct RoomData, heat_relay_state, JSON_TOK_TRUE),

};
static const struct json_obj_descr room_array_descr[] = {
    JSON_OBJ_DESCR_OBJ_ARRAY(struct RoomCollection, rooms, STRUCT_ROOM_COUNT, 
                             num_rooms, room_command_descr, ARRAY_SIZE(room_command_descr)),
};

/* End JOSN conf */

static void http_response(struct http_response_ctx *response_ctx, uint16_t status_code,
		   const void *data, size_t data_len,
		   bool final_chunk)
{
	response_ctx->status = status_code;
	response_ctx->body = data;
	response_ctx->body_len = data_len;
	response_ctx->final_chunk = final_chunk;
}

/* Polymorphic function pointer for POST parser functions */
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

	LOG_INF("POST request setting LIGHT %d to state %d", cmd.room_id, cmd.light_value);

    struct Room *room = get_room_by_id(cmd.room_id);
	if (room != NULL) {
        register_new_event(room, cmd.light_value, true);
	}
}

static void parse_temp_post(uint8_t *buf, size_t len)
{
	int ret;
	struct room_temp_set_command cmd;
	const int expected_return_code = BIT_MASK(ARRAY_SIZE(room_temp_set_command_descr));

	buf[len] = '\0';
	ret = json_obj_parse(buf, len, room_temp_set_command_descr, ARRAY_SIZE(room_temp_set_command_descr), &cmd);
	if (ret != expected_return_code) {
		LOG_WRN("Failed to fully parse JSON payload, ret=%d", ret);
		return;
	}

	LOG_INF("POST request received TEMP %d value %d", cmd.room_id, cmd.desire_temp_value);

	struct Room *room = get_room_by_id(cmd.room_id);
	if (room != NULL) {
		room->desired_temperature = cmd.desire_temp_value;
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

static struct post_state room_temp_post_state = {
	.buf = NULL,
	.cursor = 0,
	.max_size = 128,
	.parser = parse_temp_post,
};


static int post_handler(struct http_client_ctx *client, enum http_data_status status,
		       const struct http_request_ctx *request_ctx,
		       struct http_response_ctx *response_ctx, void *user_data)
{

	struct post_state *state = user_data;

	LOG_DBG("POST handler status %d, size %zu", status, request_ctx->data_len);

	if (status == HTTP_SERVER_DATA_ABORTED) {
		if (state->buf) {
			k_free(state->buf);
			state->buf = NULL;
		}
		state->cursor = 0;
		return 0;
	}

	if (state->buf == NULL) {
        state->buf = (uint8_t *)k_malloc(state->max_size + 1);
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
/* End Poly. POST */

/* Handler for GET */
static int rooms_get_handler(struct http_client_ctx *client, enum http_data_status status,
		       const struct http_request_ctx *request_ctx,
		       struct http_response_ctx *response_ctx, void *user_data)
{
	LOG_ERR("Rooms GET handler status %d", status);
	if (status == HTTP_SERVER_DATA_FINAL) {
	LOG_ERR("in if final");

		static char json_buf[512];

		struct Room **hardware_rooms = get_all_rooms();
		struct RoomCollection collection;
		collection.num_rooms = STRUCT_ROOM_COUNT;
		for (size_t i = 0; i < STRUCT_ROOM_COUNT; i++) {
			collection.rooms[i].room_id = hardware_rooms[i]->room_id;
			LOG_DBG("Room ID: %d", collection.rooms[i].room_id);
			collection.rooms[i].room_name = hardware_rooms[i]->room_name;
			LOG_DBG("Room name: %s", collection.rooms[i].room_name);
			collection.rooms[i].temp_sensor_value = hardware_rooms[i]->temp_sensor_value;
			LOG_DBG("Temp value: %d", collection.rooms[i].temp_sensor_value);
			collection.rooms[i].hum_sensor_value = hardware_rooms[i]->hum_sensor_value;
			collection.rooms[i].light_gpio_value = hardware_rooms[i]->light_gpio_value;
			collection.rooms[i].desired_temperature = hardware_rooms[i]->desired_temperature;
			collection.rooms[i].heat_relay_state = hardware_rooms[i]->heat_relay_state;
		}
		
		LOG_ERR("ALL rooms gotten");

		int ret = json_arr_encode_buf(
			room_array_descr,
			&collection,
			json_buf,
			sizeof(json_buf)
		);

		if (ret < 0) {
			LOG_ERR("Failed to encode JSON: %d", ret);
			http_response(response_ctx, 500, NULL, 0, true);
			return -1;
		}

		size_t json_len = strlen(json_buf);
		http_response(response_ctx, 200, json_buf, json_len, true);
	}
	return 0;
}

/* HTTP resource definitions */
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

static struct http_resource_detail_dynamic room_temp_resource_detail = {
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		},
	.cb = post_handler,
	.user_data = &room_temp_post_state,
};

static struct http_resource_detail_dynamic room_command_detail = {
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		},
	.cb = rooms_get_handler,
	.user_data = NULL,
};
/* END HTTP resource definitions */

/* WEB sockets */
#define MAX_WS_CLIENTS 5
static int ws_clients[MAX_WS_CLIENTS] = {0};
static uint8_t number_of_clients_connected = 0;
static uint8_t ws_buffer[128];
static uint8_t ws_tx_buffer[128];

int ws_setup(int ws_socket, struct http_request_ctx *req_ctx, void *user_data)
{
    uint64_t start_time = k_uptime_get();
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_clients[i] <= 0) {
            ws_clients[i] = ws_socket;
            LOG_INF("WebSocket client connected (slot %d)", i);
            number_of_clients_connected++;
            uint64_t end_time = k_uptime_get();
            LOG_DBG("WebSocket setup time: %llu ms", end_time - start_time);
            return 0;
        }
    }
    LOG_ERR("No free WebSocket slots");
    uint64_t end_time = k_uptime_get();
    LOG_DBG("WebSocket setup time (failure): %llu ms", end_time - start_time);
    return -ENOMEM;
}


// This thread will be responsible for sending data to all connected websocket clients
// it will not handle receiving data from clients
void ws_thread(void *arg1, void *arg2, void *arg3)
{
    (void)arg1; (void)arg2; (void)arg3;

    while (1) {
        uint64_t start_time = k_uptime_get();

        // Process all pending web events
        struct WebEvent *new_web_event = k_fifo_get(&web_events_fifo, K_NO_WAIT);
        if (new_web_event == NULL) {
            k_msleep(100);
            continue;
        }

        // Skip event processing if no clients are connected
        if (number_of_clients_connected == 0) {
            k_free(new_web_event);
            k_msleep(100);
            continue;
        }

        LOG_DBG("Sending data");
		LOG_DBG("Web event: room %d, type %d, value %d",
				new_web_event->room_id,
				new_web_event->value_type,
				new_web_event->value);

		int ret = 0;
		switch (new_web_event->value_type) {
			case LIGHT_EV: {
				struct room_light_command room_light_data;
				room_light_data.room_id = new_web_event->room_id;
				room_light_data.light_value = new_web_event->value;

				ret = json_obj_encode_buf(room_light_command_descr,
											ARRAY_SIZE(room_light_command_descr),
											&room_light_data,
											ws_tx_buffer,
											sizeof(ws_tx_buffer));
				break;
			}
			case HEAT_EV:
			case HUM_EV: {
				struct Room *r = get_room_by_id(new_web_event->room_id);
				struct room_temp_read_command room_data;
				room_data.room_id = new_web_event->room_id;
				room_data.temp_value = (new_web_event->value_type == HEAT_EV) ? new_web_event->value : (r ? r->temp_sensor_value : 0);
    			room_data.hum_value = (new_web_event->value_type == HUM_EV) ? new_web_event->value : (r ? r->hum_sensor_value : 0);
				ret = json_obj_encode_buf(room_temp_command_descr,
												ARRAY_SIZE(room_temp_command_descr),
												&room_data,
												ws_tx_buffer,
												sizeof(ws_tx_buffer));
				break;
			}
			default:
				LOG_WRN("Unknown web event type: %d", new_web_event->value_type);
				break;
		}
		k_free(new_web_event);

		if (ret < 0) {
			LOG_ERR("Encoding failed: %d", ret);
			continue;
		} else if (ret > 0) {
			LOG_DBG("Encoded successfully!");
			LOG_DBG("String: %s", ws_tx_buffer);
			LOG_HEXDUMP_DBG(ws_tx_buffer, strlen(ws_tx_buffer), "JSON raw");
		}

		// Send data to all connected clients
		for (int i = 0; i < MAX_WS_CLIENTS; i++) {
			if (ws_clients[i] == 0) continue;

			int res = websocket_send_msg(ws_clients[i], 
										ws_tx_buffer, 
										strlen(ws_tx_buffer),
										WEBSOCKET_OPCODE_DATA_TEXT,
										true,
										true,
										SYS_FOREVER_MS);

			if (res < 0) {
				LOG_INF("Client %d disconnected, freeing slot", i);
				websocket_unregister(ws_clients[i]);
				ws_clients[i] = 0;
				number_of_clients_connected--;
			}
		}
        uint64_t end_time = k_uptime_get();
        LOG_DBG("WebSocket thread processing time: %llu ms", end_time - start_time);

        k_msleep(100);
    }
}

K_THREAD_STACK_DEFINE(ws_stack, 4096);
static struct k_thread ws_tid;

static int web_init(void)
{
    k_thread_create(&ws_tid, ws_stack, K_THREAD_STACK_SIZEOF(ws_stack),
                    ws_thread, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    return 0;
}
struct http_resource_detail_websocket ws_resource_detail = {
	.common = {
			.type = HTTP_RESOURCE_TYPE_WEBSOCKET,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		},
	.cb = ws_setup,
	.data_buffer = ws_buffer,
	.data_buffer_len = sizeof(ws_buffer),
	.user_data = NULL,
};

/* END WEB sockets*/

HTTP_SERVICE_DEFINE(test_http_service, NULL, &ui_port, 4, 10, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(index_res, test_http_service, "/", &index_detail);

HTTP_RESOURCE_DEFINE(led_res, test_http_service, "/api/v1/led", &led_resource_detail);

HTTP_RESOURCE_DEFINE(light_res, test_http_service, "/api/v1/light", &room_light_resource_detail);

HTTP_RESOURCE_DEFINE(temp_res, test_http_service, "/api/v1/temp", &room_temp_resource_detail);

HTTP_RESOURCE_DEFINE(room_res, test_http_service, "/api/v1/rooms", &room_command_detail);

HTTP_RESOURCE_DEFINE(ws_res, test_http_service, "/ws", &ws_resource_detail);

SYS_INIT(web_init, APPLICATION, 0);

