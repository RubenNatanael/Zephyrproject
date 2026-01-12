#include "Room.h"

void gpio_event_action(void *ctx, uint16_t value)
{
    const struct gpio_dt_spec *gpio = ctx;
    gpio_pin_set(gpio->port, gpio->pin, value);
}

void pwm_event_action(void *ctx, uint16_t value)
{
    const struct pwm_dt_spec *pwm = ctx;
    pwm_set_dt(pwm, pwm->period, value);
}

