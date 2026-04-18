#ifndef INPUT_H
#define INPUT_H

// to read and handle input from user
// responsible for activating or deactivating the raw mode on terminal

#define CTRL_KEY(k) ((k) & 0x1f)
#define MOD_CTRL '5'
#define MOD_SHIFT '2'
#define MOD_ALT '3'
#define MOD_CTRL_SHIFT '6'

enum EditorKey {
	KEY_ARROW_UP = 1000,
	KEY_ARROW_DOWN,
	KEY_ARROW_LEFT,
	KEY_ARROW_RIGHT,
	KEY_CTRL_ARROW_UP,
	KEY_CTRL_ARROW_DOWN,
	KEY_CTRL_ARROW_RIGHT,
	KEY_CTRL_ARROW_LEFT,
	KEY_CTRL_SHIFT_ARROW_UP,
	KEY_CTRL_SHIFT_ARROW_DOWN,
	KEY_CTRL_SHIFT_ARROW_RIGHT,
	KEY_CTRL_SHIFT_ARROW_LEFT,
	KEY_ALT_ARROW_UP,
	KEY_ALT_ARROW_DOWN,
	KEY_ALT_ARROW_RIGHT,
	KEY_ALT_ARROW_LEFT,
};

void enable_raw_mode();
void disable_raw_mode();
int read_key();

#endif