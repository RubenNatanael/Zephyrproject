# Nucleo Smart Hub: Zephyr RTOS IoT System

This project is a high-performance Embedded IoT solution designed for smart home automation. Built on the Zephyr Real-Time Operating System (RTOS), it leverages a multi-threaded architecture to provide reliable, real-time control over home environments.

While currently configured for the STM32 Nucleo-F767ZI, the project is designed for hardware portability; switching to a different board simply requires updating the .overlay file.

**🚀 Features**
- Lighting Control: High-efficiency switching via GPIO, supporting transistors or relay-based actuators.

- Climate Management: Advanced temperature and humidity monitoring with intelligent relay control for heating systems.

- Dual-Interface Control:

	1. Hardware: Physical switch inputs for tactile, low-latency control.
	
	2. Web Dashboard: A modern, responsive web interface for remote monitoring and configuration.

- Automation & Scheduling: Time-based control logic for temperature setpoints (seconds-from-midnight precision).

- Connectivity & Integration:

	1. AI-Ready API: Dedicated endpoints for integration with Intelligent Agents (AI Agents, Siri, Alexa, etc.).
	
	2. Developer Console: Real-time logging and command-line control via UART/Terminal.

- Scalable Architecture: New rooms and zones are defined at compile-time using a modular Room structure pattern.

**🛠 Hardware Support**
The system is highly decoupled from the underlying hardware.

Current Target: STM32 Nucleo-F767ZI.

Porting: To use different hardware, replace the app.overlay file. The core logic relies on Zephyr's Device Tree (DTS) and Driver Model.

**📂 Configuration**
Adding a new room is straightforward. Within the source code, you define a new Room structure:

```
static struct Room lr_room  = { 
    .room_id = LIVINROOM_ROOM,
    .room_name = "Living Room",
    .light_switch = &lr_gpio_switch, 
    .light_gpio = &leds[ROOM_LED_ERROR], 
    .light_pwm = NULL, 
    .light_gpio_value = 0,
    .dht_devices = dht_devices[0],
    .dht_iodevs = &dht_iodev0,
    .temp_sensor_value = 2200,
    .hum_sensor_value = 2200,
    .desired_temperature = 2200,
    .heat_relay = &lr_gpio_relay_temp,
    .heat_relay_state = false,
    .offset_desired_temperature = 50,
    .temp_dht11 = dht11_temp_sensor,
};
```