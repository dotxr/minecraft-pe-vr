#include "vr_render.h"

#include "vr_math.h"
#include "xr_session.h"

#include <GLES/gl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cmath>
#include <vector>

#define VR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "vr-render", __VA_ARGS__)

#ifndef GL_QUADS
#define GL_QUADS 0x0007  // handled by the GLES 1 shim
#endif

namespace vr {
namespace {

// Head-locked panel that shows the GUI canvas. Positioned in view space, so it
// follows the head like a HUD and stays readable at any seat or standing height.
const float kPanelDistance = 1.8f;   // metres in front of the eyes
const float kPanelWidth = 2.0f;      // metres
const float kPanelCenterY = -0.1f;   // metres relative to eye level
const float kLaserLength = 4.0f;     // metres, when the ray misses the panel

struct EyeTarget {
    GLuint texture;
    GLuint framebuffer;
};

GLuint gEyeDepth[2];
std::vector<EyeTarget> gEyeTargets[2];

GLuint gGuiTexture;
GLuint gGuiFramebuffer;
GLuint gGuiDepth;
int gGuiWidth;
int gGuiHeight;
bool gRenderInited;

GLuint makeDepthBuffer(int width, int height) {
    GLuint rb = 0;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return rb;
}

GLuint makeFramebuffer(GLuint colorTexture, GLuint depthBuffer) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        VR_LOGE("framebuffer incomplete: 0x%x", status);
        glDeleteFramebuffers(1, &fbo);
        return 0;
    }
    return fbo;
}

GLuint framebufferFor(int eye, GLuint texture) {
    std::vector<EyeTarget>& targets = gEyeTargets[eye];
    for (size_t i = 0; i < targets.size(); ++i) {
        if (targets[i].texture == texture) return targets[i].framebuffer;
    }
    GLuint fbo = makeFramebuffer(texture, gEyeDepth[eye]);
    EyeTarget t = {texture, fbo};
    targets.push_back(t);
    return fbo;
}

// The panel's frame in room (LOCAL) space: centre, right and up axes. Head-locked, the
// frame follows the head every frame. Anchored, it stays where the player was looking
// when the screen opened, so the menu holds still while they turn.
struct PanelFrame {
    Vec3 centre;
    Vec3 right;
    Vec3 up;
};
bool gPanelAnchored = false;
PanelFrame gAnchoredFrame;

Vec3 cross(const Vec3& a, const Vec3& b) {
    return makeVec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

PanelFrame currentPanelFrame() {
    if (gPanelAnchored) return gAnchoredFrame;
    const State& st = state();
    PanelFrame f;
    f.centre = add(st.headPosition, rotate(st.headOrientation, makeVec(0, kPanelCenterY, -kPanelDistance)));
    f.right = rotate(st.headOrientation, makeVec(1, 0, 0));
    f.up = rotate(st.headOrientation, makeVec(0, 1, 0));
    return f;
}

// Room-space point to head space, which is where the panel and laser are drawn.
Vec3 toHeadSpace(const Vec3& p) {
    const State& st = state();
    return rotate(conjugate(st.headOrientation), sub(p, st.headPosition));
}

// Intersects a room-space ray with the panel. On a hit, fills the hit point and its
// position on the panel, in metres from the centre along right and up.
bool hitPanel(const PanelFrame& f, const Vec3& o, const Vec3& d, Vec3* point, float* x, float* y) {
    const Vec3 n = cross(f.right, f.up);   // faces the viewer
    const float denom = dot(d, n);
    if (denom >= -1e-4f) return false;
    const float t = dot(sub(f.centre, o), n) / denom;
    if (t < 0.0f) return false;

    const Vec3 p = add(o, scale(d, t));
    const Vec3 rel = sub(p, f.centre);
    *x = dot(rel, f.right);
    *y = dot(rel, f.up);
    const float halfW = kPanelWidth * 0.5f;
    const float halfH = halfW * (float)gGuiHeight / (float)gGuiWidth;
    if (std::fabs(*x) > halfW || std::fabs(*y) > halfH) return false;
    *point = p;
    return true;
}

}  // namespace

void renderInit(int guiWidth, int guiHeight) {
    if (gRenderInited) return;
    gRenderInited = true;
    gGuiWidth = guiWidth;
    gGuiHeight = guiHeight;

    glGenTextures(1, &gGuiTexture);
    glBindTexture(GL_TEXTURE_2D, gGuiTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, guiWidth, guiHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    gGuiDepth = makeDepthBuffer(guiWidth, guiHeight);
    gGuiFramebuffer = makeFramebuffer(gGuiTexture, gGuiDepth);

    for (int eye = 0; eye < 2; ++eye) {
        gEyeDepth[eye] = makeDepthBuffer(xr::eyeWidth(), xr::eyeHeight());
    }
}

void renderShutdown() {
    if (!gRenderInited) return;
    for (int eye = 0; eye < 2; ++eye) {
        for (size_t i = 0; i < gEyeTargets[eye].size(); ++i)
            glDeleteFramebuffers(1, &gEyeTargets[eye][i].framebuffer);
        gEyeTargets[eye].clear();
        glDeleteRenderbuffers(1, &gEyeDepth[eye]);
    }
    glDeleteFramebuffers(1, &gGuiFramebuffer);
    glDeleteRenderbuffers(1, &gGuiDepth);
    glDeleteTextures(1, &gGuiTexture);
    gRenderInited = false;
}

void beginEye(int eye, float r, float g, float b) {
    State& st = state();
    st.currentEye = -1;

    GLuint texture = xr::acquireEyeImage(eye);
    if (!texture) return;
    GLuint fbo = framebufferFor(eye, texture);
    if (!fbo) {
        xr::releaseEyeImage(eye);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, xr::eyeWidth(), xr::eyeHeight());
    glClearColor(r, g, b, 1.0f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    st.currentEye = eye;
}

void endEye(int eye) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    xr::releaseEyeImage(eye);
    state().currentEye = -1;
}

void beginGui() {
    glBindFramebuffer(GL_FRAMEBUFFER, gGuiFramebuffer);
    glViewport(0, 0, gGuiWidth, gGuiHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void endGui() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

unsigned int guiFramebufferForTest() { return gGuiFramebuffer; }

void setEyeProjection(float zNear, float zFar) {
    const Fov& f = state().eyes[state().currentEye < 0 ? 0 : state().currentEye].fov;
    float l = zNear * std::tan(f.left);
    float r = zNear * std::tan(f.right);
    float b = zNear * std::tan(f.down);
    float t = zNear * std::tan(f.up);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(l, r, b, t, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
}

Vec3 eyeOffset(int eye) {
    State& st = state();
    Vec3 d = sub(st.eyes[eye].position, st.headPosition);
    if (st.headCalibrated) d = add(d, sub(st.headPosition, st.calibrationHead));
    return rotateYaw(d, st.bodyYaw);
}

bool aimWorldDirection(Vec3* out) {
    State& st = state();
    const Hand& hand = st.hands[kRight];
    if (!isActive() || !hand.aimValid) return false;
    *out = rotateYaw(forward(hand.aimOrientation), st.bodyYaw);
    return true;
}

Vec3 aimWorldOrigin() {
    return state().headPosition;
}

bool aimAtPanel(float* guiX, float* guiY) {
    const State& st = state();
    const Hand& hand = st.hands[kRight];
    if (!hand.aimValid) return false;

    Vec3 point;
    float x = 0, y = 0;
    if (!hitPanel(currentPanelFrame(), hand.aimPosition, forward(hand.aimOrientation), &point, &x, &y))
        return false;

    const float halfW = kPanelWidth * 0.5f;
    const float halfH = halfW * (float)gGuiHeight / (float)gGuiWidth;
    // The GUI is drawn top-down, so the top edge is row 0.
    *guiX = (x + halfW) / (2.0f * halfW) * (float)gGuiWidth;
    *guiY = (halfH - y) / (2.0f * halfH) * (float)gGuiHeight;
    return true;
}

void drawPanel(int eye) {
    State& st = state();
    if (eye < 0) return;

    setEyeProjection(0.05f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Panel quad, in room space and then head space. The GUI is drawn top-down, so the
    // quad's top edge samples the top row of the canvas.
    const PanelFrame f = currentPanelFrame();
    const float halfW = kPanelWidth * 0.5f;
    const float halfH = halfW * (float)gGuiHeight / (float)gGuiWidth;
    const Vec3 corners[4] = {
        add(f.centre, add(scale(f.right, -halfW), scale(f.up, halfH))),
        add(f.centre, add(scale(f.right, -halfW), scale(f.up, -halfH))),
        add(f.centre, add(scale(f.right, halfW), scale(f.up, -halfH))),
        add(f.centre, add(scale(f.right, halfW), scale(f.up, halfH))),
    };
    float verts[4 * 3];
    for (int i = 0; i < 4; ++i) {
        const Vec3 h = toHeadSpace(corners[i]);
        verts[i * 3 + 0] = h.x;
        verts[i * 3 + 1] = h.y;
        verts[i * 3 + 2] = h.z;
    }
    const float uvs[4 * 2] = {
        0, 1,
        0, 0,
        1, 0,
        1, 1,
    };

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gGuiTexture);
    glColor4f(1, 1, 1, 1);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    glDisableClientState(GL_COLOR_ARRAY);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);

    // Laser from the right controller, in room space and drawn in head space.
    if (st.hands[kRight].aimValid) {
        const Hand& hand = st.hands[kRight];
        const Vec3 dir = forward(hand.aimOrientation);
        Vec3 end = add(hand.aimPosition, scale(dir, kLaserLength));
        Vec3 point;
        float x = 0, y = 0;
        if (hitPanel(f, hand.aimPosition, dir, &point, &x, &y)) end = point;

        const Vec3 a = toHeadSpace(hand.aimPosition);
        const Vec3 b = toHeadSpace(end);
        const float line[2 * 3] = {
            a.x, a.y, a.z,
            b.x, b.y, b.z,
        };
        glColor4f(0.45f, 0.85f, 1.0f, 0.9f);
        glVertexPointer(3, GL_FLOAT, 0, line);
        glDrawArrays(GL_LINES, 0, 2);
        glColor4f(1, 1, 1, 1);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);
}

// Anchors the panel where the player is facing, with its top kept level: the heading
// is taken from the head's yaw, so looking up or down doesn't tilt or move the menu.
void anchorPanelToView() {
    const State& st = state();
    Vec3 fwd = forward(st.headOrientation);
    fwd = makeVec(fwd.x, 0, fwd.z);
    const float len = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
    fwd = len < 1e-4f ? makeVec(0, 0, -1) : scale(fwd, 1.0f / len);

    PanelFrame f;
    f.up = makeVec(0, 1, 0);
    f.right = cross(fwd, f.up);
    f.centre = add(st.headPosition, add(scale(fwd, kPanelDistance), makeVec(0, kPanelCenterY, 0)));
    gAnchoredFrame = f;
    gPanelAnchored = true;
}

void headLockPanel() {
    gPanelAnchored = false;
}

}  // namespace vr
