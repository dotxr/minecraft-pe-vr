#include "VrKeyboard.h"

#include <cstring>

#include "Font.h"
#include "Gui.h"
#include "../Minecraft.h"
#include "../../AppPlatform.h"
#include "../../platform/input/Keyboard.h"

// Layout is in GUI units, the same space the screens draw in (the canvas divided
// by Gui::GuiScale), so the keyboard scales with the rest of the interface.
namespace {

const int kPad = 4;
const int kGap = 2;
const int kKeyH = 20;

// Special actions, kept out of the character range.
const int kActionShift = -1;
const int kActionBackspace = -2;
const int kActionEnter = -3;
const int kActionHide = -4;

const int kColourBackground = 0xE0101010;
const int kColourKey = 0xFF505050;
const int kColourKeyOn = 0xFF2F6FB0;
const int kColourText = 0xFFFFFFFF;

const char* kRows[] = {
	"1234567890",
	"qwertyuiop",
	"asdfghjkl",
	"zxcvbnm.,",
};
const int kRowCount = sizeof(kRows) / sizeof(kRows[0]);

}  // namespace

VrKeyboard::VrKeyboard() : _shift(false), _keyCount(0) {
	for (int i = 0; i < 4; ++i) _bounds[i] = 0;
}

void VrKeyboard::layout(Minecraft* mc) {
	const int sw = (int)(mc->width * Gui::InvGuiScale);
	const int sh = (int)(mc->height * Gui::InvGuiScale);
	// Ten keys across the top row set the key width for every row.
	const int keyW = (sw - 2 * kPad - 9 * kGap) / 10;

	_keyCount = 0;
	const int rowsTop = sh - (kRowCount + 1) * (kKeyH + kGap) - kPad;

	for (int r = 0; r < kRowCount; ++r) {
		const int n = (int)strlen(kRows[r]);
		const int rowW = n * keyW + (n - 1) * kGap;
		int x = (sw - rowW) / 2;
		const int y = rowsTop + r * (kKeyH + kGap);
		for (int i = 0; i < n; ++i) {
			Key& k = _keys[_keyCount++];
			k.x = x; k.y = y; k.w = keyW; k.h = kKeyH; k.action = kRows[r][i];
			x += keyW + kGap;
		}
	}

	// Bottom row: shift, space, backspace, enter, hide. Widths in key units.
	const int bottomY = rowsTop + kRowCount * (kKeyH + kGap);
	const int wide = keyW * 3 / 2;
	const int widths[] = {wide, keyW * 4, wide, wide, wide};
	const int actions[] = {kActionShift, ' ', kActionBackspace, kActionEnter, kActionHide};
	int rowW = 0;
	for (int i = 0; i < 5; ++i) rowW += widths[i] + (i ? kGap : 0);
	int x = (sw - rowW) / 2;
	for (int i = 0; i < 5; ++i) {
		Key& k = _keys[_keyCount++];
		k.x = x; k.y = bottomY; k.w = widths[i]; k.h = kKeyH; k.action = actions[i];
		x += widths[i] + kGap;
	}

	_bounds[0] = kPad;
	_bounds[1] = rowsTop - kPad;
	_bounds[2] = sw - kPad;
	_bounds[3] = bottomY + kKeyH + kPad;
}

void VrKeyboard::render(Minecraft* mc) {
	layout(mc);

	fill(_bounds[0], _bounds[1], _bounds[2], _bounds[3], kColourBackground);
	for (int i = 0; i < _keyCount; ++i) {
		const Key& k = _keys[i];
		const bool on = k.action == kActionShift && _shift;
		fill(k.x, k.y, k.x + k.w, k.y + k.h, on ? kColourKeyOn : kColourKey);

		std::string label;
		if (k.action > 0) {
			label = std::string(1, (char)k.action);
			if (_shift && label[0] >= 'a' && label[0] <= 'z') label[0] = (char)(label[0] - 32);
		} else if (k.action == kActionShift) {
			label = "shift";
		} else if (k.action == kActionBackspace) {
			label = "<-";
		} else if (k.action == kActionEnter) {
			label = "enter";
		} else if (k.action == kActionHide) {
			label = "hide";
		} else {
			label = "space";
		}
		const float tx = k.x + (k.w - mc->font->width(label)) / 2.0f;
		const float ty = k.y + (k.h - 8) / 2.0f;
		mc->font->draw(label, tx, ty, kColourText);
	}
}

bool VrKeyboard::pointerPressed(Minecraft* mc, int x, int y) {
	// The pointer arrives in GUI canvas pixels; convert to GUI units.
	const int lx = (int)(x * Gui::InvGuiScale);
	const int ly = (int)(y * Gui::InvGuiScale);
	layout(mc);
	if (lx < _bounds[0] || lx >= _bounds[2] || ly < _bounds[1] || ly >= _bounds[3]) return false;

	for (int i = 0; i < _keyCount; ++i) {
		const Key& k = _keys[i];
		if (lx >= k.x && lx < k.x + k.w && ly >= k.y && ly < k.y + k.h) {
			press(mc, k);
			break;
		}
	}
	return true;
}

void VrKeyboard::press(Minecraft* mc, const Key& key) {
	if (key.action > 0) {
		typeCharacter((char)key.action);
	} else if (key.action == kActionShift) {
		_shift = !_shift;
	} else if (key.action == kActionBackspace || key.action == kActionEnter) {
		const unsigned char code = key.action == kActionBackspace ? Keyboard::KEY_BACKSPACE : Keyboard::KEY_RETURN;
		Keyboard::feed(code, KeyboardAction::KEYDOWN);
		Keyboard::feed(code, KeyboardAction::KEYUP);
	} else if (key.action == kActionHide) {
		mc->platform()->hideKeyboard();
	}
}

void VrKeyboard::typeCharacter(char c) {
	if (_shift && c >= 'a' && c <= 'z') c = (char)(c - 32);
	Keyboard::feedText(c);
	// Shift applies to one character, as on a phone keyboard.
	_shift = false;
}
