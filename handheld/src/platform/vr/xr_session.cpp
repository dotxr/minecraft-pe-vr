#include "xr_session.h"

#include "vr_state.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <vector>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define XR_LOG(...) __android_log_print(ANDROID_LOG_INFO, "xr-session", __VA_ARGS__)
#define XR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "xr-session", __VA_ARGS__)

#define XR_CHECK_LOG(call, what)                                                     \
    do {                                                                             \
        XrResult _r = (call);                                                        \
        if (XR_FAILED(_r)) {                                                         \
            XR_LOGE("%s failed: %d", what, (int)_r);                                 \
            return false;                                                            \
        }                                                                            \
    } while (0)

namespace {

const char* const kAppName = "Minecraft PE (Quest)";
const GLenum kSwapchainFormat = GL_RGBA8;

struct Session {
    JavaVM* vm;
    jobject activity;

    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface pbuffer;

    XrInstance instance;
    XrSystemId systemId;
    XrSession session;
    XrSpace localSpace;
    XrSessionState sessionState;
    bool sessionBegun;
    bool quit;

    PFN_xrGetOpenGLESGraphicsRequirementsKHR getGlesRequirements;

    XrViewConfigurationView viewConfigs[2];
    XrSwapchain swapchains[2];
    std::vector<XrSwapchainImageOpenGLESKHR> images[2];
    uint32_t eyeWidth;
    uint32_t eyeHeight;

    XrActionSet actionSet;
    XrAction moveAction, turnAction, stickClickAction, triggerAction, gripAction;
    XrAction buttonA, buttonB, buttonX, buttonY, menuAction;
    XrAction aimPoseAction;
    XrSpace aimSpace[2];
    XrPath handPath[2];

    XrFrameState frameState;
    bool frameBegun;
    XrView views[2];
    bool eyeAcquired[2];
    bool eyeRendered[2];
};

Session xs;

bool gSessionInitialised = false;

vr::Vec3 toVec(const XrVector3f& v) { return vr::Vec3{v.x, v.y, v.z}; }
vr::Quat toQuat(const XrQuaternionf& q) { return vr::Quat{q.x, q.y, q.z, q.w}; }

vr::Fov toFov(const XrFovf& f) {
    return vr::Fov{f.angleLeft, f.angleRight, f.angleUp, f.angleDown};
}

XrPath makePath(const char* s) {
    XrPath p = XR_NULL_PATH;
    xrStringToPath(xs.instance, s, &p);
    return p;
}

bool createAction(XrActionType type, const char* name, const char* localized, XrAction* out) {
    XrActionCreateInfo info = {XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    strncpy(info.localizedActionName, localized, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    info.countSubactionPaths = 2;
    info.subactionPaths = xs.handPath;
    return XR_SUCCEEDED(xrCreateAction(xs.actionSet, &info, out));
}

bool createActions() {
    XrActionSetCreateInfo setInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy(setInfo.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
    strncpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    setInfo.priority = 0;
    XR_CHECK_LOG(xrCreateActionSet(xs.instance, &setInfo, &xs.actionSet), "xrCreateActionSet");

    xs.handPath[0] = makePath("/user/hand/left");
    xs.handPath[1] = makePath("/user/hand/right");

    if (!createAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "move", "Move", &xs.moveAction)) return false;
    if (!createAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "turn", "Turn", &xs.turnAction)) return false;
    if (!createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Stick click", &xs.stickClickAction)) return false;
    if (!createAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger", &xs.triggerAction)) return false;
    if (!createAction(XR_ACTION_TYPE_FLOAT_INPUT, "grip", "Grip", &xs.gripAction)) return false;
    if (!createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "A", &xs.buttonA)) return false;
    if (!createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "B", &xs.buttonB)) return false;
    if (!createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "X", &xs.buttonX)) return false;
    if (!createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Y", &xs.buttonY)) return false;
    if (!createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu", &xs.menuAction)) return false;
    if (!createAction(XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Aim pose", &xs.aimPoseAction)) return false;

    for (int h = 0; h < 2; ++h) {
        XrActionSpaceCreateInfo spaceInfo = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceInfo.action = xs.aimPoseAction;
        spaceInfo.subactionPath = xs.handPath[h];
        spaceInfo.poseInActionSpace.orientation.w = 1.0f;
        XR_CHECK_LOG(xrCreateActionSpace(xs.session, &spaceInfo, &xs.aimSpace[h]), "xrCreateActionSpace");
    }

    // Oculus Touch naming. Quest 3 and Quest Pro use the Meta touch-plus profile
    // with the same component paths, so suggest both.
    struct Binding { XrAction action; const char* path; };
    const Binding bindings[] = {
        {xs.moveAction, "/user/hand/left/input/thumbstick"},
        {xs.turnAction, "/user/hand/right/input/thumbstick"},
        {xs.stickClickAction, "/user/hand/left/input/thumbstick/click"},
        {xs.stickClickAction, "/user/hand/right/input/thumbstick/click"},
        {xs.triggerAction, "/user/hand/left/input/trigger/value"},
        {xs.triggerAction, "/user/hand/right/input/trigger/value"},
        {xs.gripAction, "/user/hand/left/input/squeeze/value"},
        {xs.gripAction, "/user/hand/right/input/squeeze/value"},
        {xs.buttonX, "/user/hand/left/input/x/click"},
        {xs.buttonY, "/user/hand/left/input/y/click"},
        {xs.buttonA, "/user/hand/right/input/a/click"},
        {xs.buttonB, "/user/hand/right/input/b/click"},
        {xs.menuAction, "/user/hand/left/input/menu/click"},
        {xs.aimPoseAction, "/user/hand/left/input/aim/pose"},
        {xs.aimPoseAction, "/user/hand/right/input/aim/pose"},
    };
    const size_t bindingCount = sizeof(bindings) / sizeof(bindings[0]);

    XrActionSuggestedBinding suggested[bindingCount];
    for (size_t i = 0; i < bindingCount; ++i) {
        suggested[i].action = bindings[i].action;
        suggested[i].binding = makePath(bindings[i].path);
    }

    const char* profiles[] = {
        "/interaction_profiles/oculus/touch_controller",
        "/interaction_profiles/meta/touch_controller_plus",
    };
    for (size_t p = 0; p < sizeof(profiles) / sizeof(profiles[0]); ++p) {
        XrInteractionProfileSuggestedBinding sb = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        sb.interactionProfile = makePath(profiles[p]);
        sb.countSuggestedBindings = (uint32_t)bindingCount;
        sb.suggestedBindings = suggested;
        // Some runtimes reject a profile they don't know; that's fine, the other one applies.
        XrResult r = xrSuggestInteractionProfileBindings(xs.instance, &sb);
        if (XR_FAILED(r)) XR_LOG("profile %s not suggested (%d)", profiles[p], (int)r);
    }

    XrSessionActionSetsAttachInfo attach = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1;
    attach.actionSets = &xs.actionSet;
    XR_CHECK_LOG(xrAttachSessionActionSets(xs.session, &attach), "xrAttachSessionActionSets");
    return true;
}

bool createEgl() {
    xs.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (xs.display == EGL_NO_DISPLAY) {
        XR_LOGE("eglGetDisplay failed");
        return false;
    }
    if (!eglInitialize(xs.display, NULL, NULL)) {
        XR_LOGE("eglInitialize failed: %x", eglGetError());
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE,
    };
    EGLint numConfigs = 0;
    if (!eglChooseConfig(xs.display, configAttribs, &xs.config, 1, &numConfigs) || numConfigs == 0) {
        XR_LOGE("eglChooseConfig found no GLES 3 config");
        return false;
    }

    // Ask for GLES 3.2 (Quest's native level) and fall back to 3.0.
    const EGLint ctx32[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    const EGLint ctx30[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    xs.context = eglCreateContext(xs.display, xs.config, EGL_NO_CONTEXT, ctx32);
    if (xs.context == EGL_NO_CONTEXT) xs.context = eglCreateContext(xs.display, xs.config, EGL_NO_CONTEXT, ctx30);
    if (xs.context == EGL_NO_CONTEXT) {
        XR_LOGE("eglCreateContext failed: %x", eglGetError());
        return false;
    }

    const EGLint pbufferAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    xs.pbuffer = eglCreatePbufferSurface(xs.display, xs.config, pbufferAttribs);
    if (xs.pbuffer == EGL_NO_SURFACE) {
        XR_LOGE("eglCreatePbufferSurface failed: %x", eglGetError());
        return false;
    }
    if (!eglMakeCurrent(xs.display, xs.pbuffer, xs.pbuffer, xs.context)) {
        XR_LOGE("eglMakeCurrent failed: %x", eglGetError());
        return false;
    }
    return true;
}

bool createSwapchains() {
    uint32_t viewCount = 0;
    XR_CHECK_LOG(xrEnumerateViewConfigurationViews(xs.instance, xs.systemId,
                                                   XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, NULL),
                 "xrEnumerateViewConfigurationViews");
    if (viewCount != 2) {
        XR_LOGE("expected 2 views, got %u", viewCount);
        return false;
    }
    for (uint32_t i = 0; i < 2; ++i) xs.viewConfigs[i] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    XR_CHECK_LOG(xrEnumerateViewConfigurationViews(xs.instance, xs.systemId,
                                                   XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, xs.viewConfigs),
                 "xrEnumerateViewConfigurationViews(2)");

    // Both eyes share one size so the render targets and depth buffers match.
    xs.eyeWidth = xs.viewConfigs[0].recommendedImageRectWidth;
    xs.eyeHeight = xs.viewConfigs[0].recommendedImageRectHeight;
    if (xs.viewConfigs[1].recommendedImageRectWidth > xs.eyeWidth) xs.eyeWidth = xs.viewConfigs[1].recommendedImageRectWidth;
    if (xs.viewConfigs[1].recommendedImageRectHeight > xs.eyeHeight) xs.eyeHeight = xs.viewConfigs[1].recommendedImageRectHeight;
    XR_LOG("eye swapchain %ux%u", xs.eyeWidth, xs.eyeHeight);

    for (int eye = 0; eye < 2; ++eye) {
        XrSwapchainCreateInfo info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        info.format = kSwapchainFormat;
        info.sampleCount = 1;
        info.width = xs.eyeWidth;
        info.height = xs.eyeHeight;
        info.faceCount = 1;
        info.arraySize = 1;
        info.mipCount = 1;
        XR_CHECK_LOG(xrCreateSwapchain(xs.session, &info, &xs.swapchains[eye]), "xrCreateSwapchain");

        uint32_t imageCount = 0;
        XR_CHECK_LOG(xrEnumerateSwapchainImages(xs.swapchains[eye], 0, &imageCount, NULL), "xrEnumerateSwapchainImages");
        xs.images[eye].resize(imageCount, XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        XR_CHECK_LOG(xrEnumerateSwapchainImages(xs.swapchains[eye], imageCount, &imageCount,
                                                reinterpret_cast<XrSwapchainImageBaseHeader*>(&xs.images[eye][0])),
                     "xrEnumerateSwapchainImages(images)");
    }
    return true;
}

void setPose(const XrPosef& pose, vr::Eye& eye) {
    eye.position = toVec(pose.position);
    eye.orientation = toQuat(pose.orientation);
}

// Interpolates two unit quaternions and renormalises.
vr::Quat averageOrientation(const vr::Quat& a, const vr::Quat& b) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    float s = dot < 0 ? -1.0f : 1.0f;
    vr::Quat q = {a.x + s * b.x, a.y + s * b.y, a.z + s * b.z, a.w + s * b.w};
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 0) {
        q.x /= len; q.y /= len; q.z /= len; q.w /= len;
    }
    return q;
}

void readHands(XrTime time) {
    XrActiveActionSet active = {xs.actionSet, XR_NULL_PATH};
    XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    xrSyncActions(xs.session, &sync);

    vr::State& st = vr::state();
    for (int h = 0; h < 2; ++h) {
        vr::Hand& hand = st.hands[h];
        XrPath sub = xs.handPath[h];

        XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
        XrActionStateFloat f = {XR_TYPE_ACTION_STATE_FLOAT};
        XrActionStateVector2f v = {XR_TYPE_ACTION_STATE_VECTOR2F};
        XrActionStateBoolean b = {XR_TYPE_ACTION_STATE_BOOLEAN};

        info.subactionPath = sub;
        info.action = xs.triggerAction;
        hand.trigger = (xrGetActionStateFloat(xs.session, &info, &f) == XR_SUCCESS && f.isActive) ? f.currentState : 0.0f;
        info.action = xs.gripAction;
        hand.grip = (xrGetActionStateFloat(xs.session, &info, &f) == XR_SUCCESS && f.isActive) ? f.currentState : 0.0f;

        info.action = h == vr::kRight ? xs.turnAction : xs.moveAction;
        if (xrGetActionStateVector2f(xs.session, &info, &v) == XR_SUCCESS && v.isActive) {
            hand.stickX = v.currentState.x;
            hand.stickY = v.currentState.y;
        } else {
            hand.stickX = hand.stickY = 0.0f;
        }

        info.action = xs.stickClickAction;
        hand.stickClick = xrGetActionStateBoolean(xs.session, &info, &b) == XR_SUCCESS && b.isActive && b.currentState;

        // A/B live on the right controller, X/Y and menu on the left.
        if (h == vr::kRight) {
            info.action = xs.buttonA;
            hand.buttonA = xrGetActionStateBoolean(xs.session, &info, &b) == XR_SUCCESS && b.isActive && b.currentState;
            info.action = xs.buttonB;
            hand.buttonB = xrGetActionStateBoolean(xs.session, &info, &b) == XR_SUCCESS && b.isActive && b.currentState;
        } else {
            info.action = xs.buttonX;
            hand.buttonX = xrGetActionStateBoolean(xs.session, &info, &b) == XR_SUCCESS && b.isActive && b.currentState;
            info.action = xs.buttonY;
            hand.buttonY = xrGetActionStateBoolean(xs.session, &info, &b) == XR_SUCCESS && b.isActive && b.currentState;
            info.action = xs.menuAction;
            hand.menu = xrGetActionStateBoolean(xs.session, &info, &b) == XR_SUCCESS && b.isActive && b.currentState;
        }

        XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
        hand.aimValid = false;
        hand.active = false;
        if (XR_SUCCEEDED(xrLocateSpace(xs.aimSpace[h], xs.localSpace, time, &loc)) &&
            (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
            (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
            hand.aimPosition = toVec(loc.pose.position);
            hand.aimOrientation = toQuat(loc.pose.orientation);
            hand.aimValid = true;
            hand.active = true;
        }
    }
}

}  // namespace

namespace xr {

bool init(JavaVM* vm, jobject activity) {
    if (gSessionInitialised) return true;

    xs.vm = vm;
    xs.activity = activity;
    xs.sessionState = XR_SESSION_STATE_UNKNOWN;
    xs.sessionBegun = false;
    xs.quit = false;
    xs.frameBegun = false;
    for (int i = 0; i < 2; ++i) {
        xs.eyeAcquired[i] = false;
        xs.eyeRendered[i] = false;
        xs.swapchains[i] = XR_NULL_HANDLE;
        xs.aimSpace[i] = XR_NULL_HANDLE;
    }

    PFN_xrInitializeLoaderKHR initializeLoader = NULL;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&initializeLoader);
    if (!initializeLoader) {
        XR_LOGE("no OpenXR loader on this device");
        return false;
    }
    XrLoaderInitInfoAndroidKHR loaderInit = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInit.applicationVM = vm;
    loaderInit.applicationContext = activity;
    initializeLoader((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInit);

    XrInstanceCreateInfoAndroidKHR androidInfo = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidInfo.applicationVM = vm;
    androidInfo.applicationActivity = activity;

    const char* extensions[] = {"XR_KHR_opengl_es_enable", "XR_KHR_android_create_instance"};
    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = &androidInfo;
    strncpy(createInfo.applicationInfo.applicationName, kAppName, XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    strncpy(createInfo.applicationInfo.engineName, "minecraftpe", XR_MAX_ENGINE_NAME_SIZE - 1);
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    createInfo.enabledExtensionCount = 2;
    createInfo.enabledExtensionNames = extensions;
    XR_CHECK_LOG(xrCreateInstance(&createInfo, &xs.instance), "xrCreateInstance");

    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK_LOG(xrGetSystem(xs.instance, &systemInfo, &xs.systemId), "xrGetSystem");

    xrGetInstanceProcAddr(xs.instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                          (PFN_xrVoidFunction*)&xs.getGlesRequirements);
    if (!createEgl()) return false;

    if (xs.getGlesRequirements) {
        XrGraphicsRequirementsOpenGLESKHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        xs.getGlesRequirements(xs.instance, xs.systemId, &req);
        XR_LOG("GLES min api version %d.%d", (int)XR_VERSION_MAJOR(req.minApiVersionSupported),
               (int)XR_VERSION_MINOR(req.minApiVersionSupported));
    }

    XrGraphicsBindingOpenGLESAndroidKHR binding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    binding.display = xs.display;
    binding.config = xs.config;
    binding.context = xs.context;

    XrSessionCreateInfo sessionInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = xs.systemId;
    XR_CHECK_LOG(xrCreateSession(xs.instance, &sessionInfo, &xs.session), "xrCreateSession");

    XrReferenceSpaceCreateInfo spaceInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XR_CHECK_LOG(xrCreateReferenceSpace(xs.session, &spaceInfo, &xs.localSpace), "xrCreateReferenceSpace");

    if (!createSwapchains()) return false;
    if (!createActions()) return false;

    vr::State& st = vr::state();
    st = vr::State();
    st.enabled = true;
    st.eyeWidth = (int)xs.eyeWidth;
    st.eyeHeight = (int)xs.eyeHeight;
    st.currentEye = -1;

    gSessionInitialised = true;
    XR_LOG("OpenXR session created");
    return true;
}

void shutdown() {
    if (!gSessionInitialised) return;
    for (int h = 0; h < 2; ++h) if (xs.aimSpace[h] != XR_NULL_HANDLE) xrDestroySpace(xs.aimSpace[h]);
    for (int e = 0; e < 2; ++e) if (xs.swapchains[e] != XR_NULL_HANDLE) xrDestroySwapchain(xs.swapchains[e]);
    if (xs.localSpace != XR_NULL_HANDLE) xrDestroySpace(xs.localSpace);
    if (xs.session != XR_NULL_HANDLE) xrDestroySession(xs.session);
    if (xs.instance != XR_NULL_HANDLE) xrDestroyInstance(xs.instance);
    if (xs.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(xs.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (xs.pbuffer != EGL_NO_SURFACE) eglDestroySurface(xs.display, xs.pbuffer);
        if (xs.context != EGL_NO_CONTEXT) eglDestroyContext(xs.display, xs.context);
    }
    gSessionInitialised = false;
    vr::state().enabled = false;
}

void pollEvents() {
    if (!gSessionInitialised) return;
    XrEventDataBuffer event = {XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(xs.instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const XrEventDataSessionStateChanged* changed = (const XrEventDataSessionStateChanged*)&event;
            xs.sessionState = changed->state;
            XR_LOG("session state -> %d", (int)xs.sessionState);
            switch (xs.sessionState) {
                case XR_SESSION_STATE_READY: {
                    XrSessionBeginInfo begin = {XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XR_SUCCEEDED(xrBeginSession(xs.session, &begin))) xs.sessionBegun = true;
                    break;
                }
                case XR_SESSION_STATE_STOPPING:
                    xrEndSession(xs.session);
                    xs.sessionBegun = false;
                    break;
                case XR_SESSION_STATE_EXITING:
                case XR_SESSION_STATE_LOSS_PENDING:
                    xs.quit = true;
                    break;
                default:
                    break;
            }
        } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            xs.quit = true;
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    vr::State& st = vr::state();
    st.running = xs.sessionBegun && xs.sessionState == XR_SESSION_STATE_FOCUSED;
}

bool shouldQuit() { return xs.quit; }

bool shouldRender() {
    return gSessionInitialised && xs.frameState.shouldRender;
}

void beginFrame() {
    xs.frameBegun = false;
    xs.frameState = {XR_TYPE_FRAME_STATE};
    vr::State& st = vr::state();
    st.poseValid = false;

    if (!gSessionInitialised || !xs.sessionBegun) return;

    XrFrameWaitInfo waitInfo = {XR_TYPE_FRAME_WAIT_INFO};
    if (XR_FAILED(xrWaitFrame(xs.session, &waitInfo, &xs.frameState))) return;

    XrFrameBeginInfo beginInfo = {XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(xrBeginFrame(xs.session, &beginInfo))) return;
    xs.frameBegun = true;

    XrTime time = xs.frameState.predictedDisplayTime;
    readHands(time);

    XrViewLocateInfo locateInfo = {XR_TYPE_VIEW_LOCATE_INFO};
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = time;
    locateInfo.space = xs.localSpace;

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    uint32_t viewCount = 0;
    for (int i = 0; i < 2; ++i) xs.views[i] = {XR_TYPE_VIEW};
    if (XR_FAILED(xrLocateViews(xs.session, &locateInfo, &viewState, 2, &viewCount, xs.views))) return;
    if (!(viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) ||
        !(viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
        return;
    }

    for (int eye = 0; eye < 2; ++eye) {
        setPose(xs.views[eye].pose, st.eyes[eye]);
        st.eyes[eye].fov = toFov(xs.views[eye].fov);
    }
    st.headPosition = {
        (st.eyes[0].position.x + st.eyes[1].position.x) * 0.5f,
        (st.eyes[0].position.y + st.eyes[1].position.y) * 0.5f,
        (st.eyes[0].position.z + st.eyes[1].position.z) * 0.5f,
    };
    st.headOrientation = averageOrientation(st.eyes[0].orientation, st.eyes[1].orientation);
    st.poseValid = true;

    for (int eye = 0; eye < 2; ++eye) {
        xs.eyeAcquired[eye] = false;
        xs.eyeRendered[eye] = false;
    }
}

GLuint acquireEyeImage(int eye) {
    if (!xs.frameBegun || eye < 0 || eye > 1) return 0;

    XrSwapchainImageAcquireInfo acquire = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t index = 0;
    if (XR_FAILED(xrAcquireSwapchainImage(xs.swapchains[eye], &acquire, &index))) return 0;

    XrSwapchainImageWaitInfo wait = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait.timeout = XR_INFINITE_DURATION;
    if (XR_FAILED(xrWaitSwapchainImage(xs.swapchains[eye], &wait))) {
        XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(xs.swapchains[eye], &release);
        return 0;
    }

    xs.eyeAcquired[eye] = true;
    return xs.images[eye][index].image;
}

void releaseEyeImage(int eye) {
    if (eye < 0 || eye > 1 || !xs.eyeAcquired[eye]) return;
    XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(xs.swapchains[eye], &release);
    xs.eyeAcquired[eye] = false;
    xs.eyeRendered[eye] = true;
}

void endFrame() {
    if (!xs.frameBegun) return;

    XrCompositionLayerProjectionView projViews[2];
    XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = xs.localSpace;
    layer.viewCount = 2;
    layer.views = projViews;

    bool allRendered = xs.eyeRendered[0] && xs.eyeRendered[1];
    for (int eye = 0; eye < 2; ++eye) {
        projViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projViews[eye].pose = xs.views[eye].pose;
        projViews[eye].fov = xs.views[eye].fov;
        projViews[eye].subImage.swapchain = xs.swapchains[eye];
        projViews[eye].subImage.imageRect.offset = {0, 0};
        projViews[eye].subImage.imageRect.extent = {(int32_t)xs.eyeWidth, (int32_t)xs.eyeHeight};
        projViews[eye].subImage.imageArrayIndex = 0;
    }

    const XrCompositionLayerBaseHeader* layers[1] = {(const XrCompositionLayerBaseHeader*)&layer};

    XrFrameEndInfo endInfo = {XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = xs.frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    // Without both eyes drawn there is nothing valid to show, so submit no layer.
    endInfo.layerCount = allRendered ? 1 : 0;
    endInfo.layers = allRendered ? layers : NULL;
    xrEndFrame(xs.session, &endInfo);

    xs.frameBegun = false;
    for (int eye = 0; eye < 2; ++eye) xs.eyeRendered[eye] = false;
}

int eyeWidth() { return (int)xs.eyeWidth; }
int eyeHeight() { return (int)xs.eyeHeight; }

}  // namespace xr
