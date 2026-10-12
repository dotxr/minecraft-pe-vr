// Reference: desktop GL 2.1 compatibility profile (Mesa llvmpipe), same fixed-function model as GLES 1.1.
#include <EGL/egl.h>
#include "desktop_compat.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

void renderScene(int w, int h);

int main(int argc, char** argv) {
    const int W = 256, H = 256;
    EGLDisplay d = eglGetPlatformDisplay(0x31DD /* EGL_PLATFORM_SURFACELESS_MESA */, EGL_DEFAULT_DISPLAY, NULL);
    if (!eglInitialize(d, NULL, NULL)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
    eglBindAPI(EGL_OPENGL_API);
    const EGLint cfgAttr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                              EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(d, cfgAttr, &cfg, 1, &n);
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 1,
                              EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT, EGL_NONE};
    EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, ctxAttr);
    const EGLint pbAttr[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
    EGLSurface s = eglCreatePbufferSurface(d, cfg, pbAttr);
    if (!eglMakeCurrent(d, s, s, c)) { fprintf(stderr, "eglMakeCurrent failed %x\n", eglGetError()); return 1; }

    renderScene(W, H);

    std::vector<unsigned char> px(W * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    FILE* f = fopen(argv[1], "wb");
    fwrite(&px[0], 1, px.size(), f);
    fclose(f);
    printf("reference GL error: 0x%x\n", glGetError());
    return 0;
}
