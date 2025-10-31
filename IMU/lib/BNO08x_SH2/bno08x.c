#include "bno08x.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "sh2_err.h"

// Static (private) variables
static bno08x_t *_bno_instance = NULL;
static sh2_SensorValue_t *_sensor_value = NULL;
static bool _reset_flag = false;

// --- Helper Functions ---
static void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop");
}

static void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

// --- SH2 HAL Implementation for RP2040/RP2350 ---

static bool spihal_wait_for_int(void) {
    // Wait up to 250ms for the interrupt pin to go low
    for (int i = 0; i < 250; i++) {
        if (!gpio_get(_bno_instance->pins.int_pin)) {
            return true;
        }
        sleep_ms(1);
    }
    printf("Timed out waiting for interrupt pin.\n");
    return false;
}

static void hal_hardware_reset_internal(void) {
    if (_bno_instance->pins.rst_pin != -1) {
        printf("BNO08x Hardware reset\n");
        gpio_put(_bno_instance->pins.rst_pin, 0);
        sleep_ms(100);
        gpio_put(_bno_instance->pins.rst_pin, 1);
        sleep_ms(100);
    }
}

static int spihal_open(sh2_Hal_t *self) {
    // Initialize GPIO for CS, INT, and RST
    gpio_init(_bno_instance->pins.cs_pin);
    gpio_set_dir(_bno_instance->pins.cs_pin, GPIO_OUT);
    cs_deselect(_bno_instance->pins.cs_pin);

    gpio_init(_bno_instance->pins.int_pin);
    gpio_set_dir(_bno_instance->pins.int_pin, GPIO_IN);
    gpio_pull_up(_bno_instance->pins.int_pin);

    if (_bno_instance->pins.rst_pin != -1) {
        gpio_init(_bno_instance->pins.rst_pin);
        gpio_set_dir(_bno_instance->pins.rst_pin, GPIO_OUT);
        gpio_put(_bno_instance->pins.rst_pin, 1);
    }
    
    hal_hardware_reset_internal();

    // *********************************************************************
    // THE FIX: Do NOT wait for an interrupt here.
    // The BNO085 does not seem to send a "booted" interrupt.
    // We will wait for interrupts during read/write operations instead.
    // spihal_wait_for_int();  // <--- THIS LINE IS REMOVED
    // *********************************************************************

    return 0;
}

static void spihal_close(sh2_Hal_t *self) {
    spi_deinit(_bno_instance->pins.spi_port);
}

static int spihal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    *t_us = time_us_32();
    uint16_t packet_size = 0;

    if (!spihal_wait_for_int()) {
        return 0;
    }

    uint8_t header[4];
    cs_select(_bno_instance->pins.cs_pin);
    sleep_us(10); // SETUP TIME
    spi_read_blocking(_bno_instance->pins.spi_port, 0x00, header, 4);
    sleep_us(10); // HOLD TIME
    cs_deselect(_bno_instance->pins.cs_pin);

    packet_size = (uint16_t)header[0] | ((uint16_t)header[1] << 8);
    packet_size &= ~0x8000;

    if (packet_size == 0) return 0;
    if (packet_size > len) {
        printf("Error: Packet size (%d) > buffer size (%d)\n", packet_size, len);
        uint8_t temp_buf[packet_size];
        cs_select(_bno_instance->pins.cs_pin);
        sleep_us(10);
        spi_read_blocking(_bno_instance->pins.spi_port, 0x00, temp_buf, packet_size);
        sleep_us(10);
        cs_deselect(_bno_instance->pins.cs_pin);
        return 0;
    }

    cs_select(_bno_instance->pins.cs_pin);
    sleep_us(10);
    spi_read_blocking(_bno_instance->pins.spi_port, 0x00, pBuffer, packet_size);
    sleep_us(10);
    cs_deselect(_bno_instance->pins.cs_pin);

    return packet_size;
}

static int spihal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    if (!spihal_wait_for_int()) {
        return 0;
    }

    cs_select(_bno_instance->pins.cs_pin);
    sleep_us(10); // SETUP TIME
    spi_write_blocking(_bno_instance->pins.spi_port, pBuffer, len);
    sleep_us(10); // HOLD TIME
    cs_deselect(_bno_instance->pins.cs_pin);

    return len;
}

static uint32_t hal_get_time_us(sh2_Hal_t *self) {
    return time_us_32();
}

static void hal_callback(void *cookie, sh2_AsyncEvent_t *pEvent) {
    if (pEvent->eventId == SH2_RESET) {
        _reset_flag = true;
    }
}

static void sensor_handler(void *cookie, sh2_SensorEvent_t *event) {
    if (_sensor_value == NULL) return;
    
    if (sh2_decodeSensorEvent(_sensor_value, event) != SH2_OK) {
        _sensor_value->timestamp = 0;
    }
}

// --- Public API Implementation ---

bool bno08x_begin_spi(bno08x_t *self, bno08x_pins_t pins_config) {
    _bno_instance = self;
    self->pins = pins_config;
    
    // Using a slow, robust SPI speed that we know works.
    spi_init(self->pins.spi_port, 300 * 1000); 
    spi_set_format(self->pins.spi_port, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    
    gpio_set_function(self->pins.sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(self->pins.mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function(self->pins.miso_pin, GPIO_FUNC_SPI);

    self->hal.open = spihal_open;
    self->hal.close = spihal_close;
    self->hal.read = spihal_read;
    self->hal.write = spihal_write;
    self->hal.getTimeUs = hal_get_time_us;

    int status = sh2_open(&self->hal, hal_callback, NULL);
    if (status != SH2_OK) {
        printf("sh2_open failed with status: %d\n", status);
        return false;
    }

    memset(&self->prodIds, 0, sizeof(self->prodIds));
    status = sh2_getProdIds(&self->prodIds);
    if (status != SH2_OK) {
        printf("sh2_getProdIds failed with status: %d\n", status);
        return false;
    }

    sh2_setSensorCallback(sensor_handler, NULL);

    return true;
}

// ... (rest of the public functions: bno08x_hardware_reset, was_reset, get_sensor_event, enable_report) ...
// ... (They are unchanged and can be copied from the previous correct version if needed) ...

void bno08x_hardware_reset(bno08x_t *self) {
    _bno_instance = self;
    hal_hardware_reset_internal();
}

bool bno08x_was_reset(bno08x_t *self) {
    _bno_instance = self;
    bool x = _reset_flag;
    _reset_flag = false;
    return x;
}

bool bno08x_get_sensor_event(sh2_SensorValue_t *value) {
    _sensor_value = value;
    value->timestamp = 0; 
    sh2_service();
    return (value->timestamp != 0);
}

bool bno08x_enable_report(sh2_SensorId_t sensorId, uint32_t interval_us) {
    static sh2_SensorConfig_t config;
    config.changeSensitivityEnabled = false;
    config.wakeupEnabled = false;
    config.changeSensitivityRelative = false;
    config.alwaysOnEnabled = false;
    config.changeSensitivity = 0;
    config.batchInterval_us = 0;
    config.sensorSpecific = 0;
    config.reportInterval_us = interval_us;

    if (sh2_setSensorConfig(sensorId, &config) != SH2_OK) {
        return false;
    }
    return true;
}