#ifndef PLATFORM_VR_VR_STATE_H__
#define PLATFORM_VR_VR_STATE_H__

// Plain-data view of the headset and controllers, filled in once per frame by
// the OpenXR layer (xr_session.cpp) and read by the renderer and input code.
// Nothing here depends on OpenXR headers, so the game code stays portable.
//
// Coordinates: the OpenXR LOCAL space (right-handed, +Y up, -Z forward). Game
// (world) space is the same up-axis, rotated about Y by state().bodyYaw.

namespace vr {

struct Vec3 {
    float x, y, z;
};

struct Quat {
    float x, y, z, w;
};

// Field of view in radians. left and down are negative.
struct Fov {
    float left, right, up, down;
};

struct Eye {
    Vec3 position;
    Quat orientation;
    Fov fov;
};

struct Hand {
    bool active;          // controller is tracked this frame
    bool aimValid;        // aim pose is usable (ray origin and direction)
    Vec3 aimPosition;
    Quat aimOrientation;
    float trigger;        // 0..1
    float grip;           // 0..1
    float stickX, stickY; // -1..1, +X right, +Y up
    bool stickClick;
    bool buttonA, buttonB, buttonX, buttonY;  // A/B on right, X/Y on left
    bool menu;
};

enum { kLeft = 0, kRight = 1 };

struct State {
    bool enabled;         // OpenXR session was created
    bool running;         // session is focused and the game may update and render
    bool poseValid;       // eye and head poses are valid this frame

    Eye eyes[2];
    Vec3 headPosition;    // centre between the eyes, LOCAL space
    Quat headOrientation;
    Hand hands[2];

    // Game yaw in degrees that the room's forward direction is rotated by.
    // Stick turning changes this; head yaw is added on top of it.
    float bodyYaw;
    bool bodyYawAligned;  // set once bodyYaw has been matched to the game's yaw

    int eyeWidth, eyeHeight;  // swapchain size per eye
    int guiWidth, guiHeight;  // logical GUI canvas, also the game's width/height

    int currentEye;       // eye being rendered, or -1
};

State& state();

inline bool isActive() { return state().enabled && state().running; }

}  // namespace vr

#endif /*PLATFORM_VR_VR_STATE_H__*/
