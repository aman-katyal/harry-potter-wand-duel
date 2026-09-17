cmake_minimum_required(VERSION 3.13)

# Initialize Pico SDK
include(pico_sdk_import.cmake)

project(proton)

pico_sdk_init()

# Add your source files
add_executable(proton
    main.c
    ei_bridge.cpp
    lib/Adafruit_BNO08x/bno08x_driver.c
    lib/Adafruit_BNO08x/sh2.c
    lib/Adafruit_BNO08x/sh2_hal.c
    lib/Adafruit_BNO08x/sh2_SensorValue.c
    lib/Adafruit_BNO08x/sh2_util.c
    lib/Adafruit_BNO08x/shtp.c
    lib/Adafruit_Bus_IO/adafruit_busio_register.c
    lib/Adafruit_Bus_IO/adafruit_i2cdev.c
    lib/Adafruit_Unified_Sensor/adafruit_sensor.c
    lib/haptics/drv2605.c
    lib/ir_receiver/ir_decode_nec.c
    lib/ir_receiver/ir_rp_hal.c
    lib/ir_transmitter/ir_emitter.c
    lib/led_matrix/animation_params.c
    lib/led_matrix/circle_explosion.c
    lib/led_matrix/controller.c
    lib/led_matrix/firework.c
    lib/led_matrix/healthbar.c
    lib/led_matrix/led_effects.c
    lib/led_matrix/ripple.c
    lib/led_matrix/spiral.c
    lib/led_matrix/ws2812.c
)

# --- THE FIX IS HERE ---
# You must link 'hardware_watchdog' to use watchdog_enable()
target_link_libraries(proton
    pico_stdlib
    pico_multicore
    hardware_i2c
    hardware_pwm
    hardware_pio
    hardware_watchdog  # <--- ADD THIS LINE
)

# Your USB Configuration
pico_enable_stdio_usb(proton 1)
pico_enable_stdio_uart(proton 0)

# Create .uf2 file
pico_add_extra_outputs(proton)