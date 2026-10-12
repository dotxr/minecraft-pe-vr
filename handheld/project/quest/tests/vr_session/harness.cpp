// Desktop harness for the VR layer. It drives the real session (xr_session.cpp built with
// XR_HOST_TEST), the eye and GUI render targets (vr_render.cpp) and the GLES 1 shim against
// an OpenXR runtime (Monado, headless), renders a few hundred frames, and checks that the
// GUI panel lands in both eye images on every frame.
#include "platform/gles1_shim.h"
#include "platform/vr/vr_render.h"
#include "platform/vr/vr_state.h"
#include "platform/vr/xr_session.h"

#include <GLES/gl.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <unistd.h>
#include <vector>

namespace {

const int kGuiW = 1280, kGuiH = 720;
const int kFrames = 300;

// The GUI page: a solid red canvas drawn with the shim's client arrays, as the game does.
void drawGuiPage() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, kGuiW, kGuiH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    // The panel leaves back-face culling on, and this quad's winding flips under the
    // y-down projection, so the quad would be culled without this line.
    glDisable(GL_CULL_FACE);
    glColor4f(1, 0, 0, 1);
    const float quad[] = {0, 0, 0, kGuiW, 0, 0, kGuiW, kGuiH, 0, 0, kGuiH, 0};
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, quad);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// Green world geometry, in front of the viewer and behind the panel.
void drawEyeWorld() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -2);
    glColor4f(0, 1, 0, 1);
    const float tri[] = {-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0};
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, tri);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
}

unsigned char centreRed(int w, int h) {
    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return px[0];
}

// Counts the red pixels of the GUI canvas, to confirm the whole page was drawn.
long redPixelsInGui() {
    std::vector<unsigned char> px((size_t)kGuiW * kGuiH * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, vr::guiFramebufferForTest());
    glReadPixels(0, 0, kGuiW, kGuiH, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    long reds = 0;
    for (size_t i = 0; i < px.size(); i += 4)
        if (px[i] > 200 && px[i + 1] < 50) ++reds;
    return reds;
}

}  // namespace

int main() {
    if (!xr::init(NULL, NULL)) {
        fprintf(stderr, "xr::init failed\n");
        return 2;
    }
    gles1ShimInit();
    vr::renderInit(kGuiW, kGuiH);

    int framesSeen = 0, framesRendered = 0, panelHits = 0, panelMisses = 0;
    long lastGuiRed = 0;
    for (int frame = 0; frame < kFrames && !xr::shouldQuit(); ++frame) {
        xr::pollEvents();
        xr::beginFrame();
        ++framesSeen;

        if (vr::isActive() && xr::shouldRender() && vr::state().poseValid) {
            vr::beginGui();
            drawGuiPage();
            vr::endGui();
            if (frame == kFrames - 1) lastGuiRed = redPixelsInGui();

            for (int eye = 0; eye < 2; ++eye) {
                vr::beginEye(eye, 0.1f, 0.1f, 0.1f);
                if (vr::state().currentEye < 0) continue;
                vr::setEyeProjection(0.05f, 100.0f);
                drawEyeWorld();
                vr::drawPanel(eye);
                // The head-locked panel is opaque red at the centre of each eye.
                if (centreRed(xr::eyeWidth(), xr::eyeHeight()) > 200) ++panelHits; else ++panelMisses;
                vr::endEye(eye);
            }
            ++framesRendered;
        } else {
            usleep(10000);
        }
        xr::endFrame();
    }

    printf("frames=%d rendered=%d gui_red_pixels=%ld/%d panel_hits=%d panel_misses=%d eye=%dx%d\n",
           framesSeen, framesRendered, lastGuiRed, kGuiW * kGuiH, panelHits, panelMisses,
           xr::eyeWidth(), xr::eyeHeight());
    vr::renderShutdown();
    xr::shutdown();

    const bool ok = framesRendered > 0 && panelMisses == 0 && lastGuiRed == (long)kGuiW * kGuiH;
    printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
