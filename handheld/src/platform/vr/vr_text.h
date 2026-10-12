#ifndef PLATFORM_VR_VR_TEXT_H__
#define PLATFORM_VR_VR_TEXT_H__

// Text the game asks for (world name, seed) when the Android dialog isn't available.
// The headset keyboard (client/gui/VrKeyboard) answers the request, one field at a
// time, and the platform reports the result exactly as the Android dialog would.

#include <string>
#include <vector>

namespace vr {
namespace text {

// Starts a request for the dialog. Fields come from the dialog id: a world asks for a
// name and then an optional seed, the rename asks for a name.
void begin(int dialogId);

// True while the player is typing into a request.
bool open();

// -1 while open, 1 when the player finished, 0 when they cancelled. Stays set after
// the request closes, until the next begin().
int status();

// The entered values, one per field, valid once status() is not -1.
std::vector<std::string> result();

// The field being typed, its prompt, and where it is in the request.
std::string& currentValue();
const char* currentPrompt();

// Enter: the next field, or the end of the request after the last one.
void advance();
void cancel();

}  // namespace text
}  // namespace vr

#endif /*PLATFORM_VR_VR_TEXT_H__*/
