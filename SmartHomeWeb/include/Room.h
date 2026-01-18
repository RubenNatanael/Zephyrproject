#ifndef ROOMS_H
#define ROOMS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/rtio/rtio.h>


extern struct k_fifo events_fifo;
extern struct k_fifo web_events_fifo;

enum {
    ROOM_LED_POWER,
    ROOM_LED_INFO,
    ROOM_LED_ERROR,
    ROOM_LED_COUNT
};

enum {
    LIVINROOM_ROOM,
    KITCHEN_ROOM,
    STRUCT_ROOM_COUNT
};

enum VALUE_TYPE {
    SWITCH_EV,
    LIGHT_EV,
    HEAT_EV,
    HUM_EV,
    COUNT_EV
};

typedef void (*event_action_t)(void *ctx, uint16_t value);

struct Event {
    void *fifo_reserved;
    event_action_t action;
    void *ctx;
    uint16_t value;
};

struct WebEvent {
    void *fifo_reserved;
    int room_id;
    enum VALUE_TYPE value_type;
    uint32_t value;
};

struct Room {
    void* fifo_reserved;

    const uint8_t room_id;
    const char* room_name;

    /* Light */
    /* Switch can toggle pwm device or gpio*/
    const struct gpio_dt_spec* light_switch;
    const struct pwm_dt_spec* light_pwm;
    const struct gpio_dt_spec* light_gpio;
    uint16_t light_value;

    /* TODO Heat */
    const struct device *const dht_devices;
    struct rtio_iodev *const dht_iodevs;
    uint32_t last_temp_value;
    uint32_t last_hum_value;
    /* Temperature sensor */
    /* Temperature gpio */
};

void gpio_event_action(void *ctx, uint16_t value);

void pwm_event_action(void *ctx, uint16_t value);

bool room_device_init();

struct Room** get_all_rooms();

struct Room* get_room_by_id(int id);

const struct gpio_dt_spec* get_led_by_id(int id);

bool register_new_event(struct Room *room, uint16_t new_value, bool is_for_web_event);

int read_temp_and_hum(struct Room *room, uint32_t* temp_fit, uint32_t* hum_fit);

bool register_new_temp_hum_event(struct Room *room, uint32_t temp_value, uint32_t hum_value, bool is_for_web_event);
#endif