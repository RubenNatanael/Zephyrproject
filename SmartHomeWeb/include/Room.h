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
    SETPOINT_EV,
    HEAT_RELAY_EV,
    COUNT_EV,
    NONE_EV
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
    const struct gpio_dt_spec* light_switch;   // INPUT
    const struct pwm_dt_spec* light_pwm;       // OUTPUT PWM
    const struct gpio_dt_spec* light_gpio;     // OUTPUT GPIO
    uint16_t light_gpio_value;                 // Current GPIO value

/* Heat */
    /* Sensor and RTIO IODEV for DHT22 */
    const struct device *const dht_devices;    // INPUT DHT device
    struct rtio_iodev *const dht_iodevs;       // RTIO IODEV for DHT
    uint32_t temp_sensor_value;                // Last read temperature
    uint32_t hum_sensor_value;                 // Last read humidity
    /* Actuators */
    const struct gpio_dt_spec* heat_relay;     // OUTPUT HEAT relay GPIO
    bool heat_relay_state;                     // OUTPUT HEAT relay state
    uint32_t desired_temperature;              // Desired temperature

    uint32_t offset_desired_temperature;       // Offset for desired temperature
                                               // VALUES: 25  - 0.25 C
                                               //         50  - 0.50 C
                                               //         75  - 0.75 C
                                               //         100 - 1.00 C
};

void gpio_event_action(void *ctx, uint16_t value);

void pwm_event_action(void *ctx, uint16_t value);

bool room_device_init();

struct Room** get_all_rooms();

struct Room* get_room_by_id(int id);

const struct gpio_dt_spec* get_led_by_id(int id);

int register_new_event(struct Room *room, uint16_t new_value, enum VALUE_TYPE event_type, bool is_for_web_event);

int read_temp_and_hum(struct Room *room, uint32_t* temp_fit, uint32_t* hum_fit);

bool register_new_web_event(uint32_t room_id, enum VALUE_TYPE value_type, uint32_t value);

#endif