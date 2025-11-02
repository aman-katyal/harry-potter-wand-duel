#include <stdio.h>
#include "pico/stdlib.h"
#include "drv2605.h"

// --- I2C HAPTIC MOTOR CONFIGURATION ---
#define I2C_PORT i2c0
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17
// --- End Configuration ---


/**
 * @brief Loads a sequence of haptic effects into the DRV2605 and plays it.
 * 
 * @param drv Pointer to the initialized drv2605_t driver object.
 * @param effects A constant array of effect IDs (1-123). The sequence is played in order.
 * @param num_effects The number of effects in the array (can be up to 8).
 */
void play_haptic_sequence(drv2605_t *drv, const uint8_t *effects, int num_effects) {
    // The DRV2605 can sequence up to 8 waveforms.
    if (num_effects > 8) {
        num_effects = 8;
    }

    // Load the effect IDs into the hardware waveform sequencer registers.
    for (int i = 0; i < num_effects; i++) {
        drv2605_set_waveform(drv, i, effects[i]);
    }
    
    // Add the "end of sequence" marker (0) after the last effect.
    drv2605_set_waveform(drv, num_effects, 0);

    // Fire the sequence!
    drv2605_go(drv);
}


int main() {
    stdio_init_all();
    sleep_ms(3000);

    printf("--- DRV2605 Custom Waveform Sequencer ---\n");

    drv2605_t drv;

    if (!drv2605_init(&drv, I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN)) {
        printf("ERROR: DRV2605 not found.\n");
        while (1) tight_loop_contents();
    }
    printf("DRV2605 found.\n");

    // --- SETUP (Done Once) ---
    // Set the mode to be triggered by the internal "GO" command.
    drv2605_set_mode(&drv, DRV2605_MODE_INTTRIG);
    // Select Waveform Library 1 (the main one for ERM motors).
    drv2605_select_library(&drv, 1);
    
    printf("Ready to play custom sequences.\n");


    // --- DEFINE YOUR CUSTOM HAPTIC SEQUENCE HERE ---
    // To change the effect, just change the numbers in this array.
    // Effect #84 = "Transition Ramp Up Medium Smooth 1 (0 to 100%)"
    // Effect #1  = "Strong Click - 100%"
    const uint8_t notification_buzz[] = {84};
    int num_effects = sizeof(notification_buzz) / sizeof(notification_buzz[0]);

    /* --- EXAMPLE of a different sequence you could try ---
    // Effect #10 = "Double Click - 100%"
    // Effect #10 = "Double Click - 100%"
    const uint8_t double_double_click[] = {10, 10};
    int num_effects = sizeof(double_double_click) / sizeof(double_double_click[0]);
    */


    // --- LOOP ---
    // This will play your defined sequence over and over.
    while (1) {
        printf("Playing notification buzz sequence...\n");
        play_haptic_sequence(&drv, notification_buzz, num_effects);
        
        // Wait 2 seconds before playing again.
        sleep_ms(2000);
    }

    return 0;
}