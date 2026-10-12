#ifndef PLATFORM_VR_VR_RENDER_H__
#define PLATFORM_VR_VR_RENDER_H__

#include "vr_state.h"

// Render targets and presentation for the headset: one framebuffer per eye
// swapchain image, one offscreen GUI canvas, and the head-locked panel the GUI
// canvas is drawn onto.
namespace vr {

// Creates the GUI canvas. Call once after the GL context is current.
void renderInit(int guiWidth, int guiHeight);
void renderShutdown();

// Acquires the eye's swapchain image, binds it, and clears it.
// Sets state().currentEye. Pair with endEye().
void beginEye(int eye, float r, float g, float b);
void endEye(int eye);

// Binds the GUI canvas (transparent, sized guiWidth x guiHeight).
void beginGui();
void endGui();

// Draws the GUI canvas as a head-locked panel and the controller laser on top
// of the current eye. Call inside beginEye/endEye, after the world.
void drawPanel(int eye);

// Projection for the eye being rendered (replaces gluPerspective in VR).
void setEyeProjection(float zNear, float zFar);

// Offset of the eye from the head centre, in world space (game yaw applied, pitch not).
Vec3 eyeOffset(int eye);

// Where the right controller's aim ray hits the GUI panel, in GUI canvas pixels.
// Returns false if it misses.
bool aimAtPanel(float* guiX, float* guiY);

}  // namespace vr

#endif /*PLATFORM_VR_VR_RENDER_H__*/
