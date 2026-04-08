#ifndef INPUT_H
#define INPUT_H

// to read and handle input from user

enum {
	KEY_ARROW_UP,
	KEY_ARROW_DOWN,
	KEY_ARROW_LEFT,
	KEY_ARROW_RIGHT
};

void enable_raw_mode();
void disable_raw_mode();
int read_key();

#endif