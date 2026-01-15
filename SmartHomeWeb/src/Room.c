#include "Room.h"

LOG_MODULE_REGISTER(room, LOG_LEVEL_DBG);

K_FIFO_DEFINE(events_fifo);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec leds[ROOM_LED_COUNT] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
};

static const struct pwm_dt_spec lr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmlivingroom));
static const struct pwm_dt_spec kr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmkitchen));

static const struct gpio_dt_spec lr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchlivingroom), gpios, {0});
static const struct gpio_dt_spec kr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchkitchen), gpios, {0});

static struct Room lr_room  = { 
    .light_switch = &lr_gpio_switch, 
    .light_gpio = &leds[ROOM_LED_ERROR], 
    .light_pwm = NULL, 
    .light_value = 0 
};
static struct Room kr_room = { 
    .light_switch = &kr_gpio_switch, 
    .light_gpio = NULL, 
    .light_pwm = &kr_pwdled, 
    .light_value = 0
};

static struct Room *rooms[STRUCT_ROOM_COUNT] = { 
    &lr_room, 
    &kr_room 
};

bool room_device_init() {
    int ret = 0;

    /* Leds init */
    for (int i = 0; i < ROOM_LED_COUNT; i++) {
        if (!gpio_is_ready_dt(&leds[i])) return false;
        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_ACTIVE);
    }

    /* PWM signal for leds init */
    if (!device_is_ready(lr_pwdled.dev) || !device_is_ready(kr_pwdled.dev)) {
        LOG_ERR("PWM devices not ready");
        return false;
    }


    /* Switch init */
    const struct gpio_dt_spec *switches[] = { &lr_gpio_switch, &kr_gpio_switch};
    int number_of_switches = sizeof(switches) / sizeof(struct gpio_dt_spec*);
    for (int i = 0; i < number_of_switches; i++) {
        if (!gpio_is_ready_dt(switches[i])) return 0;
        ret = gpio_pin_configure_dt(switches[i], GPIO_INPUT | switches[i]->dt_flags);
        if (ret != 0) {
            LOG_ERR("Configuring kit GPIO pin failed: %d", ret);
            return false;
        }
    }

	LOG_INF("Initialization and configuration switch done.");
    return true;
}
 
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

struct Room** get_all_rooms() {
    return rooms;
}

struct Room* get_room_by_id(int id) {
    return rooms[id];
}

const struct gpio_dt_spec* get_led_by_id(int id) {
    return &leds[id];
}

