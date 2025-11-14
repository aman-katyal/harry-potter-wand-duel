#ifndef HEALTHBAR_H
#define HEALTHBAR_H

#include <stdint.h>

void hb_init(int width, int height);
void hb_draw(void);                // draw the bar to the LED buffer
void hb_update(int delta);         // +healing / -damage
int  hb_get_health(void);          // optional helper

#endif
