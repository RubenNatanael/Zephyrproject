#include "PHSwitch.h"

PHSwitch::PHSwitch(const struct gpio_dt_spec* gpio_dt_spec, const char* name_) {
    this->device = gpio_dt_spec;
    this->name_ = name_;

    if (!gpio_is_ready_dt(device)) {
        printk("The %s switch pin GPIO port is not ready.\n", this->name_);
        return;
    }
    int ret = gpio_pin_configure_dt(device, GPIO_OUTPUT | device->dt_flags);
    if (ret != 0) {
        printk("Configuring %s GPIO pin failed: %d\n", this->name_, ret);
        return;
    }
}

bool PHSwitch::is_pressed() {
    return gpio_pin_get_dt(device);
}

const struct gpio_dt_spec* PHSwitch::getDevice() {
    return device;
}
