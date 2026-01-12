#ifndef ROOMS_H
#define ROOMS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <stddef.h>
#include <string.h>

typedef void (*event_action_t)(void *ctx, uint16_t value);

struct Event {
    void *fifo_reserved;
    event_action_t action;
    void *ctx;
    uint16_t value;
};

struct Room {
    void* fifo_reserved;

    /* Light */
    /* Switch can toggle pwm device or gpio*/
    const struct gpio_dt_spec* light_switch;
    const struct pwm_dt_spec* light_pwm;
    const struct gpio_dt_spec* light_gpio;
    uint16_t light_value;

    /* TODO Heat */
    /* Temperature sensor */
    /* Temperature gpio */
};

void gpio_event_action(void *ctx, uint16_t value);

void pwm_event_action(void *ctx, uint16_t value);

#endif