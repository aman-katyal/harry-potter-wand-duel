// int main() {
//     stdio_init_all();
//     ws2812_init();

//     hb_init(WIDTH, HEIGHT);
    
//     // Start by showing the health bar
//     hb_draw();
//     sleep_ms(300);

//     uint8_t spell = 0;

//     while (true) {

//         // 1. Apply damage BEFORE animation (only once per spell)
//         switch (spell) {
//             case 0:
//             case 1:
//             case 2:
//                 hb_update(-2);   // fireworks
//                 break;
//             case 3:
//                 hb_update(-1);   // spiral
//                 break;
//             case 4:
//                 hb_update(-3);   // explosion
//                 break;
//         }

//         // 2. If dead → show loser screen + reset
//         if (hb_current() == 0) {
//             loser_screen(WIDTH, HEIGHT);
//             sleep_ms(800);
//             hb_reset();
//         }

//         // 3. Clear the screen *before* the animation
//         ws2812_clear();

//         // 4. Now run the animation (on a blank screen)
//         controller(spell, WIDTH, HEIGHT);

//         // 5. Animation is done, draw *only* the health bar
//         hb_draw();

//         // 6. Next spell
//         spell++;
//         if (spell > 4) spell = 0;
            
//         sleep_ms(500);
//     }
// }