#ifndef HEALTHBAR_H
#define HEALTHBAR_H

#include <stdint.h>

void hb_init(int width, int height);
void hb_draw(void);          
void hb_update(int delta);        
int  hb_get_health(void);         
void hb_reset(void);
void loser_screen(int width, int height);
int hb_current(void);



#endif
