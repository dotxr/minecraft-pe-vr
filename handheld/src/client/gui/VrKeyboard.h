#ifndef NET_MINECRAFT_CLIENT_GUI__VrKeyboard_H__
#define NET_MINECRAFT_CLIENT_GUI__VrKeyboard_H__

// On-screen keyboard for the headset. The Android soft keyboard can't be used in
// VR, so while the platform reports the keyboard as shown, this overlay is drawn
// over the GUI canvas and the controller ray types into it. Keys are delivered
// through the same Keyboard queues a physical keyboard uses, so screens such as
// the sign editor need no VR-specific code.

#include "GuiComponent.h"

class Minecraft;

class VrKeyboard: public GuiComponent
{
public:
	VrKeyboard();

	// Draws the keyboard into the GUI canvas, at the bottom of the screen.
	void render(Minecraft* mc);

	// A pointer press at GUI canvas coordinates. Returns true if it landed on the
	// keyboard, so the caller doesn't also send it to the screen underneath.
	bool pointerPressed(Minecraft* mc, int x, int y);

private:
	struct Key {
		int x, y, w, h;
		int action;   // a character, or one of the kAction values
	};
	void layout(Minecraft* mc);
	void press(Minecraft* mc, const Key& key);
	void typeCharacter(char c);

	bool _shift;
	Key _keys[64];
	int _keyCount;
	int _bounds[4];   // left, top, right, bottom of the whole overlay
};

#endif /*NET_MINECRAFT_CLIENT_GUI__VrKeyboard_H__*/
