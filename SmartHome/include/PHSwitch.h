#ifndef PHSWITCH_H
#define PHSWITCH_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

class PHSwitch {
public:
    PHSwitch(const struct gpio_dt_spec*, const char* name_);

    bool is_pressed();

    const struct gpio_dt_spec* getDevice();

private:
    const struct gpio_dt_spec *device;
    const char* name_;
};

#endif