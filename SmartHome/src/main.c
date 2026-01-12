#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <stddef.h>
#include <string.h>

#include "Room.h"

#define SLEEP_TIME_MS 200
#define STACKSIZE 1024
#define PRIORITY 7

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec power_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec power_led_info = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec power_led_error = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

static const struct pwm_dt_spec lr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmlivingroom));
static const struct pwm_dt_spec kr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmkitchen));

static const struct gpio_dt_spec lr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchlivingroom), gpios, {0});
static const struct gpio_dt_spec kr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchkitchen), gpios, {0});

static const struct Room lr_room  = { 
    .light_switch = &lr_gpio_switch, 
    .light_gpio = &power_led_error, 
    .light_pwm = NULL, 
    .light_value = 0 
};
static const struct Room kr_room = { 
    .light_switch = &kr_gpio_switch, 
    .light_gpio = NULL, 
    .light_pwm = &kr_pwdled, 
    .light_value = 0
 };

K_FIFO_DEFINE(events_fifo);

void log_msg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

void listening_events_thread(void) {

    const struct Room *rooms[] = { &lr_room, &kr_room };
    int num_rooms = sizeof(rooms) / sizeof(rooms[0]);

    while (1) {
        int percentage_ = 50;

        for (int i = 0; i < num_rooms; i++) {

            bool new_state = gpio_pin_get_dt(rooms[i]->light_switch);

            struct Event *new_event = k_malloc(sizeof(struct Event));
            if (!new_event) {
                printk("Unable to allocate memory for event\n");
                return;
            }

            /* Register light events */
            if (rooms[i]->light_gpio != NULL) {
                new_event->action = gpio_event_action;
                new_event->ctx = (void *)rooms[i]->light_gpio;
                new_event->value = new_state;
            }
            if (rooms[i]->light_pwm != NULL) {
                // PWM light needs special value so I calculated it here
                new_state = rooms[i]->light_pwm->period * percentage_ / 100;
                new_event->action = pwm_event_action;
                new_event->ctx = (void *)rooms[i]->light_pwm;
                new_event->value = new_state;
            }

            // TODO register heat events

            k_fifo_put(&events_fifo, new_event);
        }

        k_msleep(SLEEP_TIME_MS);
    }
}

void execut_events_thread(void) {
    while (1) {
        struct Event *registered_event = k_fifo_get(&events_fifo,
							   K_FOREVER);
        registered_event->action(
            registered_event->ctx,
            registered_event->value
        );

        k_free(registered_event);

        k_msleep(SLEEP_TIME_MS);
    }
}

int main(void)
{
    printk("Booting C++ Zephyr LightSwitch app\n");

    int ret = 0;

    const struct gpio_dt_spec *leds[] = { &power_led, &power_led_info, &power_led_error };
    int number_of_gpios = sizeof(leds) / sizeof(struct gpio_dt_spec*);
    for (int i = 0; i < number_of_gpios; i++) {
        if (!gpio_is_ready_dt(leds[i])) return 0;
        gpio_pin_configure_dt(leds[i], GPIO_OUTPUT_ACTIVE);
    }

    for (int i = 0; i < 5; ++i) {
        gpio_pin_toggle_dt(&power_led);
        k_msleep(500);
    }

    if (!device_is_ready(lr_pwdled.dev) || !device_is_ready(kr_pwdled.dev)) {
        log_msg("PWM devices not ready\n");
        return 0;
    }


    /*
    Switch settup
    */
    const struct gpio_dt_spec *switches[] = { &lr_gpio_switch, &kr_gpio_switch};
    int number_of_switches = sizeof(switches) / sizeof(struct gpio_dt_spec*);
    for (int i = 0; i < number_of_switches; i++) {
        if (!gpio_is_ready_dt(switches[i])) return 0;
        ret = gpio_pin_configure_dt(switches[i], GPIO_INPUT | switches[i]->dt_flags);
        if (ret != 0) {
            printk("Configuring kit GPIO pin failed: %d\n", ret);
            return 0;
        }
    }

	log_msg("Initialization and configuration switch done.\n");

    while (1) {

        ret = gpio_pin_toggle_dt(&power_led);
        if (ret < 0) {
            return -1;
        }
        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
    
}

// --- Thread definitions ---
K_THREAD_DEFINE(listening_id, STACKSIZE, listening_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
K_THREAD_DEFINE(execut_id, STACKSIZE, execut_events_thread, NULL, NULL, NULL,
                PRIORITY, 0, 0);
