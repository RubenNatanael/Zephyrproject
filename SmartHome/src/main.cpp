#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <string.h>

#define SLEEP_TIME_MS 1000
#define STACKSIZE 1024
#define PRIORITY 7

void log_msg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

struct Event {
    void* fifo_reserved;
    bool is_gpio;
    const pwm_dt_spec* pwm_device;
    const gpio_dt_spec* gpio_device;
    uint16_t value;
};

extern "C" K_FIFO_DEFINE(events_fifo);

class PWMLED {
public:
    PWMLED(const pwm_dt_spec* pwm) : pwm_(pwm) {}
    void set(uint16_t value) { pwm_set_dt(pwm_, pwm_->period, value); }
    uint32_t period() const { return pwm_->period; }
private:
    const pwm_dt_spec* pwm_;
};

class GPIOLED {
public:
    GPIOLED(const gpio_dt_spec* gpio) : gpio_(gpio) {}
    void toggle() { gpio_pin_toggle_dt(gpio_); }
    void set(bool state) { gpio_pin_set(gpio_->port, gpio_->pin, state); }
private:
    const gpio_dt_spec* gpio_;
};

class Switch {
public:
    Switch(const gpio_dt_spec* gpio, const pwm_dt_spec* pwm_device = nullptr, const gpio_dt_spec* gpio_led = nullptr, uint16_t percentage = 50)
        : gpio_(gpio), pwm_(pwm_device), gpio_led_(gpio_led), percentage_(percentage), status_(false) {}

    void poll() {
        bool state = gpio_pin_get_dt(gpio_);
        uint32_t val = 0;

        if (pwm_) {
            val = state ? pwm_->period * percentage_ / 100 : 0;
        } else if (gpio_led_) {
            val = state ? 1 : 0;
        }

        if (state != status_) {
            status_ = state;
            Event* ev = (Event*)k_malloc(sizeof(Event));
            if (!ev) return;
            ev->is_gpio = (gpio_led_ != nullptr);
            ev->pwm_device = pwm_;
            ev->gpio_device = gpio_led_;
            ev->value = val;
            log_msg("New event registered\n");
            k_fifo_put(&events_fifo, ev);
        }
    }
    const gpio_dt_spec* getGpio() {
        return gpio_;
    }

private:
    const gpio_dt_spec* gpio_;
    const pwm_dt_spec* pwm_;
    const gpio_dt_spec* gpio_led_;
    uint16_t percentage_;
    bool status_;
};

// --- Hardware definitions ---
static const gpio_dt_spec power_led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const gpio_dt_spec info_led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const gpio_dt_spec error_led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static const pwm_dt_spec lr_pwm_spec = PWM_DT_SPEC_GET(DT_ALIAS(pwmlivingroom));
static const pwm_dt_spec kr_pwm_spec = PWM_DT_SPEC_GET(DT_ALIAS(pwmkitchen));

static const gpio_dt_spec lr_switch_gpio = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchlivingroom), gpios, {0});
static const gpio_dt_spec kr_switch_gpio = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchkitchen), gpios, {0});

PWMLED lr_pwm(&lr_pwm_spec);
PWMLED kr_pwm(&kr_pwm_spec);

GPIOLED power_led(&power_led_spec);
GPIOLED info_led(&info_led_spec);
GPIOLED error_led(&error_led_spec);

Switch lr_switch(&lr_switch_gpio, &lr_pwm_spec, nullptr);
Switch kr_switch(&kr_switch_gpio, &kr_pwm_spec, nullptr);

extern "C" void listening_thread(void*) {
    Switch *switches[] = { &lr_switch, &kr_switch };

    while (true) {
        printk("listening\n");
        for (auto sw : switches) {
            sw->poll();
        }
        k_msleep(3020);
    }
}

extern "C" void execute_thread(void*) {
    while (true) {
        printk("event\n");
        Event* ev = (Event*)k_fifo_get(&events_fifo, K_FOREVER);
        if (!ev) continue;

        if (!ev->is_gpio && ev->pwm_device) {
            pwm_set_dt(ev->pwm_device, ev->pwm_device->period, ev->value);
        } else if (ev->is_gpio && ev->gpio_device) {
            gpio_pin_set(ev->gpio_device->port, ev->gpio_device->pin, ev->value);
        }

        k_free(ev);
        k_msleep(4330);
    }
}

extern "C" int main() {
    printk("Booting C++ Zephyr LightSwitch app\n");

    const gpio_dt_spec leds[] = { power_led_spec, info_led_spec, error_led_spec };
    for (auto& led : leds) {
        if (!gpio_is_ready_dt(&led)) return 0;
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }

    for (int i = 0; i < 5; ++i) {
        power_led.toggle();
        k_msleep(500);
    }

    if (!device_is_ready(lr_pwm_spec.dev) || !device_is_ready(kr_pwm_spec.dev)) {
        log_msg("PWM devices not ready\n");
        return 0;
    }

    // Configure switch GPIOs
   Switch *switches[] = { &lr_switch, &kr_switch };
    for (auto sw : switches) {
        if (!gpio_is_ready_dt(sw->getGpio())) return 0;
        gpio_pin_configure_dt(sw->getGpio(), GPIO_INPUT | sw->getGpio()->dt_flags);
    }

    int i = 0;
    while (true) {
        power_led.toggle();
        if (i % 2 == 0) info_led.toggle();
        if (i % 4 == 0) error_led.toggle();
        ++i;
        log_msg("Status led\n");
        k_msleep(SLEEP_TIME_MS);
    }
}

// --- Thread definitions ---
K_THREAD_DEFINE(listening_id, STACKSIZE, listening_thread, nullptr, nullptr, nullptr,
                PRIORITY, 0, 0);
K_THREAD_DEFINE(execut_id, STACKSIZE, execute_thread, nullptr, nullptr, nullptr,
                PRIORITY, 0, 0);
