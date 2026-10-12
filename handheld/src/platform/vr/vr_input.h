#ifndef PLATFORM_VR_VR_INPUT_H__
#define PLATFORM_VR_VR_INPUT_H__

class IInputHolder;
class NinecraftApp;
class Minecraft;
class LocalPlayer;

// Controller input for the headset. The game reads it through the same
// IInputHolder interfaces as the other platforms, so nothing in the game logic
// knows it is running in VR.
namespace vr {

// Input holder used while the headset is active.
IInputHolder* createInputHolder();

// Once per frame, before the game update: feeds the GUI pointer and handles the
// menu button.
void updateInput(NinecraftApp* app, Minecraft* mc);

// Once per frame, before the game update: turns the player to match the head
// (plus stick turning) so the camera and movement follow the headset.
void syncPlayerToHead(LocalPlayer* player);

}  // namespace vr

#endif /*PLATFORM_VR_VR_INPUT_H__*/
