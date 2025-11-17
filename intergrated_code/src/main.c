    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "ir_emitter.h"
    #include "irremote.h"
    #include "drv2605.h"

    // Pin definitions
    #define TX_PIN 36
    #define RX_PIN 15
    #define I2C_SDA 16
    #define I2C_SCL 17

    // Button Definitions
    #define BTN_A 21
    #define BTN_B 26

    // ===================================
    // GAME CONFIGURATION
    // ===================================
    // Set these for each wand!
    // Wand 1: MY=1, OPPONENT=2
    // Wand 2: MY=2, OPPONENT=1
    #define MY_PLAYER_ID 1
    #define OPPONENT_PLAYER_ID 2

    #define PACKET_REPEATS 3 
    #define MAX_HEALTH 16    
    // ===================================

    // Packet validation
    #define PACKET_BUFFER_SIZE 3
    static uint8_t packet_buffer[PACKET_BUFFER_SIZE] = {0};
    static int packet_index = 0;

    // --- Helper Functions ---

    bool validate_spell(uint8_t spell_num) {
        int match_count = 0;
        for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
            if (packet_buffer[i] == spell_num) {
                match_count++;
            }
        }
        return (match_count >= 2);
    }

    void add_packet(uint8_t spell_num) {
        packet_buffer[packet_index] = spell_num;
        packet_index = (packet_index + 1) % PACKET_BUFFER_SIZE;
    }

    void clear_packets() {
        for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
            packet_buffer[i] = 0;
        }
        packet_index = 0;
    }

    void init_buttons() {
        // Active High (Press = 1)
        gpio_init(BTN_A); gpio_set_dir(BTN_A, GPIO_IN); gpio_pull_down(BTN_A);
        gpio_init(BTN_B); gpio_set_dir(BTN_B, GPIO_IN); gpio_pull_down(BTN_B);
    }

    int get_spell_damage(uint8_t spell_num) {
        switch(spell_num) {
            case 1: return 1;
            case 2: return 2;
            case 3: return 3;
            default: return 0;
        }
    }

    int main() {
        stdio_init_all();
        sleep_ms(3000);
        
        printf("\n=== Wand Duel ===\n");
        
        init_buttons();
        ir_emitter_init(TX_PIN);
        ir_receiver_init(RX_PIN);
        
        drv2605_t haptic;
        drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
        drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
        drv2605_select_library(&haptic, 1);
        
        int current_health = MAX_HEALTH;

        printf("Wand Ready!\n");
        printf("--> My ID: %d\n", MY_PLAYER_ID);
        printf("--> Accepting hits from ID: %d\n", OPPONENT_PLAYER_ID);
        printf("Health: %d/%d\n\n", current_health, MAX_HEALTH);
        
        ir_decoded_data_t received_spell;
        bool currently_casting = false;
        uint8_t last_spell = 0;
        
        while (true) {
            // ==========================================
            // 1. RECEIVE SPELLS LOGIC
            // ==========================================
            if (ir_receiver_decode(&received_spell)) {
                // Check 1: Is it the right protocol?
                if (received_spell.protocol == IR_PROTOCOL_NEC) {
                    
                    // Check 2: Is it from our opponent?
                    if (received_spell.address == OPPONENT_PLAYER_ID) {
                        
                        uint8_t spell_num = received_spell.command;
                        add_packet(spell_num);
                    
                        if (validate_spell(spell_num) && spell_num != last_spell) {
                            printf("\n*** HIT CONFIRMED! (from %d) ***\n", received_spell.address);
                            drv2605_play_hit_feedback(&haptic);
                            current_health -= get_spell_damage(spell_num);
                            printf("Health: %d/%d\n", current_health, MAX_HEALTH);
                            
                            last_spell = spell_num;
                            clear_packets();
                            
                            if (current_health <= 0) {
                                printf("*** DEFEATED ***\n");
                                sleep_ms(2000);
                                current_health = MAX_HEALTH;
                                last_spell = 0;
                            }
                        }
                    } 
                    // Check 3: Is it just our own spell? (Ignore)
                    else if (received_spell.address == MY_PLAYER_ID) {
                        // printf("Ignored own spell packet.\n");
                    }
                    // Check 4: Is it from an unknown wand? (Ignore)
                    else {
                        printf("Ignored packet from unknown address: %d\n", received_spell.address);
                    }
                }
            }
            
            // ==========================================
            // 2. EMITTER UPDATE LOOP
            // ==========================================
            ir_emitter_update();
            
            if (currently_casting && ir_emitter_done()) {
                printf("Spell sent.\n");
                currently_casting = false;
            }

            // ==========================================
            // 3. BUTTON INPUT LOGIC (CASTING)
            // ==========================================
            
            if (!currently_casting && ir_emitter_done()) {
                
                bool a_pressed = gpio_get(BTN_A);
                bool b_pressed = gpio_get(BTN_B);

                if (a_pressed || b_pressed) {
                    sleep_ms(50); // Combo delay
                    a_pressed = gpio_get(BTN_A);
                    b_pressed = gpio_get(BTN_B);
                    
                    int spell_to_cast = 0;

                    if (a_pressed && b_pressed) spell_to_cast = 3;
                    else if (a_pressed) spell_to_cast = 1;
                    else if (b_pressed) spell_to_cast = 2;

                    if (spell_to_cast > 0) {
                        printf("CASTING Spell %d (as ID %d)\n", spell_to_cast, MY_PLAYER_ID);
                        drv2605_play_cast_feedback(&haptic);
                        
                        // Transmit using MY_PLAYER_ID
                        ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                        
                        currently_casting = true;
                        sleep_ms(400); // Debounce
                    }
                }
            }
            sleep_ms(1);
        }
    }