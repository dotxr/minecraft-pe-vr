#ifndef PLATFORM_VR_XR_SESSION_H__
#define PLATFORM_VR_XR_SESSION_H__

#include <jni.h>
#include <GLES3/gl3.h>

// OpenXR session for the Quest port: instance, EGL context, swapchains, the
// controller action set and the frame loop. The game only sees vr::State and
// the eye/GUI render targets from vr_render.h.
//
// Per frame, from the native main loop:
//   xr::pollEvents();
//   if (xr::shouldQuit()) break;
//   xr::beginFrame();          // waits for the frame, locates views and hands
//   if (xr::shouldRender()) app->update();   // renders eyes through vr_render.h
//   xr::endFrame();            // submits every eye that was rendered
namespace xr {

// Returns false (after logging why) if the headset can't be used.
bool init(JavaVM* vm, jobject activity);
void shutdown();

void pollEvents();
bool shouldQuit();
bool shouldRender();  // true when the headset wants frames this beginFrame()

void beginFrame();
void endFrame();

// Swapchain images for one eye. Acquire before drawing into the eye, release
// after. Returns the GL texture name to attach to a framebuffer.
GLuint acquireEyeImage(int eye);
void releaseEyeImage(int eye);

int eyeWidth();
int eyeHeight();

}  // namespace xr

#endif /*PLATFORM_VR_XR_SESSION_H__*/
