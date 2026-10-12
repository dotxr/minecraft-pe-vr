#ifndef PLATFORM_GLES1_SHIM_H__
#define PLATFORM_GLES1_SHIM_H__

// GLES 1.1 fixed-function entry points implemented on top of an OpenGL ES 3.0
// context. OpenXR on Quest only hands out GLES 3 contexts, so the renderer's
// GLES 1 calls are satisfied by gles1_shim.cpp instead of libGLESv1_CM.
//
// Call gles1ShimInit() once a GLES 3 context is current, before any drawing.
void gles1ShimInit();

#endif /*PLATFORM_GLES1_SHIM_H__*/
