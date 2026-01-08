#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

#define SLEEP_TIME_MS 200
#define STACKSIZE 1024
#define PRIORITY 7

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

struct event {
    void *fifo_reserved;
    bool is_gpio_device;
    struct pwm_dt_spec *pwm_device;
    struct gpio_dt_spec *gpio_device;
    uint16_t value;
};

struct switches {
    struct gpio_dt_spec *switch_device;
    bool status;
};

K_FIFO_DEFINE(events_fifo);

static const struct gpio_dt_spec power_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec power_led_info = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec power_led_error = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

static const struct pwm_dt_spec lr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmlivingroom));
static const struct pwm_dt_spec kr_pwdled = PWM_DT_SPEC_GET(DT_ALIAS(pwmkitchen));

static const struct gpio_dt_spec lr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchlivingroom), gpios, {0});
static const struct gpio_dt_spec kr_gpio_switch = GPIO_DT_SPEC_GET_OR(DT_ALIAS(switchkitchen), gpios, {0});
static const struct switches lr_switch  = { .switch_device = &lr_gpio_switch, .status = false };
static const struct switches kr_switch = { .switch_device = &kr_gpio_switch, .status = false };


static uint32_t light_lr = 0;
static uint32_t light_kt = 0;


void log_msg(const char* fmt, ...);


typedef void (*switch_callback_t)(uint32_t);
void getSwitchValue(struct switches *sw, switch_callback_t callback);


#define GET_SWITCH_VALUE(sw, code_block)              \
    ({                                               \
        static void __tmp_cb(uint32_t val) { code_block; } \
        getSwitchValue(sw, __tmp_cb);               \
    })



void listening_events(void) {
    while (1) {

        GET_SWITCH_VALUE(&lr_switch,
            {
                struct event *new_event = k_malloc(sizeof(struct event));
                if (!new_event) return;
                new_event->is_gpio_device = false;
                new_event->pwm_device = &lr_pwdled;
                new_event->value = val;
                k_fifo_put(&events_fifo, new_event);
            }
        );
        GET_SWITCH_VALUE(&kr_switch,
            {
                truct event *new_event = k_malloc(sizeof(struct event));
                if (!new_event) return;
                new_event->is_gpio_device = false;
                new_event->pwm_device = &kr_pwdled;
                new_event->value = val;
                k_fifo_put(&events_fifo, new_event);
            }
        );

        k_msleep(SLEEP_TIME_MS);
    }
}

void execut_events(void) {
    while (1) {
        struct event *new_event = k_fifo_get(&events_fifo,
							   K_FOREVER);
        if (!new_event->is_gpio_device) { // PWM
            pwm_set_dt(new_event->pwm_device, new_event->pwm_device->period, new_event->value);
        } else { // GPIO
            gpio_pin_set(new_event->gpio_device->port, new_event->gpio_device->pin, new_event->value);
        }

        k_free(new_event);

        k_msleep(SLEEP_TIME_MS);
    }
}

int main(void)
{
    printk("boot\n");
    int ret;
    bool led_state = true;

    if (!gpio_is_ready_dt(&power_led)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&power_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        log_msg("Error while configuring the led\n");
        return -1;
    }

    if (!gpio_is_ready_dt(&power_led_info)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&power_led_info, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        log_msg("Error while configuring the led\n");
        return -1;
    }

    if (!gpio_is_ready_dt(&power_led_error)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&power_led_error, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        log_msg("Error while configuring the led\n");
        return -1;
    }


    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);
    ret = gpio_pin_toggle_dt(&power_led);
    k_msleep(500);

    /*
    Led settup
    */
   if (!device_is_ready(lr_pwdled.dev)) {
        log_msg("LivingRoom led not ready\n");
        return 0;
    }
    if (!device_is_ready(kr_pwdled.dev)) {
        log_msg("Kitchen led not ready\n");
        return 0;
    }
	log_msg("Initialization light done.\n");

    /*
    Switch settup
    */
   if (!gpio_is_ready_dt(&lr_switch)) {
        printk("The Livin switch pin GPIO port is not ready.\n");
        return;
    }
    ret = gpio_pin_configure_dt(&lr_switch, GPIO_INPUT | lr_switch.dt_flags);
    if (ret != 0) {
        printk("Configuring Living GPIO pin failed: %d\n", ret);
        return;
    }
    if (!gpio_is_ready_dt(&kr_switch)) {
        printk("The Kit switch pin GPIO port is not ready.\n");
        return;
    }
    ret = gpio_pin_configure_dt(&kr_switch, GPIO_INPUT | kr_switch.dt_flags);
    if (ret != 0) {
        printk("Configuring kit GPIO pin failed: %d\n", ret);
        return;
    }

	log_msg("Initialization and configuration switch done.\n");

    int i = 0;

    while (1) {

        ret = gpio_pin_toggle_dt(&power_led);
        if (ret < 0) {
            return -1;
        }
        led_state = !led_state;
        log_msg("State of LED changed to: %s\n", led_state ? "ON" : "OFF");
        if (i % 2 == 0)
            ret = gpio_pin_toggle_dt(&power_led_info);
        if (i++ % 4 == 0)
            ret = gpio_pin_toggle_dt(&power_led_error);

        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
    
}

K_THREAD_DEFINE(listening_id, STACKSIZE, listening_events, NULL, NULL, NULL,
    PRIORITY, 0, 0);
K_THREAD_DEFINE(executing_id, STACKSIZE, execut_events, NULL, NULL, NULL,
    PRIORITY, 0, 0);

void getSwitchValue(struct switches *sw, switch_callback_t callback) {
    uint16_t procentage = 50; // getOnlineState(); // request procentage of light from app or local,
                              // now the value is hardcoded to 50%
    uint32_t val;
    bool rsp = gpio_pin_get_dt(sw->switch_device);
    if (rsp) {
        val = lr_pwdled.period * procentage / 100;
    } else {
        val = 0;
    }
    // Value must be different in order to run the callback
    if (callback && rsp != sw->status) {
        sw->status = rsp;
        callback(val);
    }
}

void log_msg(const char* fmt, ...) {
    /*
    Use this function to log also in a file or a circular buffer or a serial terminal/device,...
    */
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
