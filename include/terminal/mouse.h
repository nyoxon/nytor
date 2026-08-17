#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_BUTTON_LEFT 0
#define MOUSE_BUTTON_MIDDLE 1
#define MOUSE_BUTTON_RIGHT 2

#define MOUSE_SCROLL_UP 64
#define MOUSE_SCROLL_DOWN 65

#define MOUSE_MOVING 32

struct mouse_event {
	int button;
	int x; // 1 based
	int y; // 1 based

	int pressed;
};

void mouse_on();
void mouse_off();

void mouse_get_event();

#endif