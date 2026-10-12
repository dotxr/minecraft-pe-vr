// Under test: the shim on a GLES 3 context.
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "gles1_shim.h"

void renderScene(int w, int h);

int main(int argc, char** argv) {
    const int W = 256, H = 256;
    EGLDisplay d = eglGetPlatformDisplay(0x31DD, EGL_DEFAULT_DISPLAY, NULL);
    if (!eglInitialize(d, NULL, NULL)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint cfgAttr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                              EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(d, cfgAttr, &cfg, 1, &n);
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, ctxAttr);
    const EGLint pbAttr[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
    EGLSurface s = eglCreatePbufferSurface(d, cfg, pbAttr);
    if (!eglMakeCurrent(d, s, s, c)) { fprintf(stderr, "eglMakeCurrent failed %x\n", eglGetError()); return 1; }

    gles1ShimInit();
    renderScene(W, H);

    std::vector<unsigned char> px(W * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    FILE* f = fopen(argv[1], "wb");
    fwrite(&px[0], 1, px.size(), f);
    fclose(f);
    printf("shim GL error: 0x%x\n", glGetError());
    return 0;
}
