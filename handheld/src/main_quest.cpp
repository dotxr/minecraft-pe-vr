// Native entry point for the Meta Quest build (OpenXR, NativeActivity).
//
// Replaces the EGL window path in main_android.cpp. The headset provides the
// GL context and the swapchains, so this file owns the frame loop:
//   poll OpenXR events -> wait for a frame -> run the game (which renders both
//   eyes) -> submit the frame.

#include "App.h"
#include "AppPlatform_android.h"
#include "NinecraftApp.h"
#include "platform/gles1_shim.h"
#include "platform/vr/vr_input.h"
#include "platform/vr/vr_render.h"
#include "platform/vr/vr_state.h"
#include "platform/vr/xr_session.h"

#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <unistd.h>

#define MAIN_CLASS NinecraftApp

// Size of the GUI canvas that the HUD and menus are drawn into (16:9).
static const int kGuiWidth = 1280;
static const int kGuiHeight = 720;

static AppPlatform_android appPlatform;

// Worlds live in the app's own external directory (Android/data/<package>/files), which
// needs no storage permission. Shared storage is not writable from this target SDK.
static void setupExternalPath(JNIEnv* env, jobject activity, MAIN_CLASS* app)
{
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getFilesDir = env->GetMethodID(activityClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    jobject file = env->CallObjectMethod(activity, getFilesDir, NULL);
    if (env->ExceptionOccurred()) env->ExceptionDescribe();
    if (file == NULL) {
        LOGE("getExternalFilesDir returned null; worlds cannot be saved");
        return;
    }

    jclass fileClass = env->GetObjectClass(file);
    jmethodID fileMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jobject pathString = env->CallObjectMethod(file, fileMethod);

    const char* str = env->GetStringUTFChars((jstring) pathString, NULL);
    app->externalStoragePath = str;
    app->externalCacheStoragePath = str;
    LOGI("external storage: %s", str);
    env->ReleaseStringUTFChars((jstring) pathString, str);
}

static void handleAppCommand(struct android_app* state, int32_t cmd) {
    if (cmd == APP_CMD_DESTROY) state->destroyRequested = 1;
}

static int32_t handleInputEvent(struct android_app* state, AInputEvent* event) {
    // Controllers arrive through OpenXR. Only the Android back key is handled here.
    return 0;
}

extern "C" {

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnLoad");
    return appPlatform.init(vm);
}

// The Java activity calls these from onCreate/onStop/onDestroy. The VR session
// is owned by android_main, so they only need to exist.
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeRegisterThis(JNIEnv* env, jobject clazz) {}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeUnregisterThis(JNIEnv* env, jobject clazz) {}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeStopThis(JNIEnv* env, jobject clazz) {
    LOGI("activity stopped");
}

}  // extern "C"

void android_main(struct android_app* state)
{
    JavaVM* vm = state->activity->vm;
    JNIEnv* env = NULL;
    vm->AttachCurrentThread(&env, NULL);

    jobject activity = env->NewGlobalRef(state->activity->clazz);
    appPlatform.instance = activity;
    appPlatform.initConsts();

    state->onAppCmd = handleAppCommand;
    state->onInputEvent = handleInputEvent;

    if (!xr::init(vm, activity)) {
        LOGE("OpenXR initialisation failed; the headset is required");
        // Close the activity rather than leave a black window with no way out.
        ANativeActivity_finish(state->activity);
        vm->DetachCurrentThread();
        return;
    }

    gles1ShimInit();
    vr::renderInit(kGuiWidth, kGuiHeight);

    AppContext context;
    context.doRender = false;
    context.platform = &appPlatform;

    MAIN_CLASS* app = new MAIN_CLASS();
    setupExternalPath(env, activity, app);

    static_cast<App*>(app)->init(context);
    app->setSize(kGuiWidth, kGuiHeight);

    bool wasActive = false;
    while (!xr::shouldQuit() && !state->destroyRequested) {
        // Android events (lifecycle, back key) are delivered through the looper.
        int ident;
        int events;
        struct android_poll_source* source;
        while ((ident = ALooper_pollAll(0, NULL, &events, (void**) &source)) >= 0) {
            if (source != NULL) source->process(state, source);
        }

        xr::pollEvents();
        if (xr::shouldQuit()) break;

        xr::beginFrame();

        // Headset taken off or focus lost while playing: open the pause menu.
        bool active = vr::isActive();
        if (wasActive && !active && app->level != NULL) app->pauseGame(true);
        wasActive = active;

        if (active && xr::shouldRender() && vr::state().poseValid) {
            app->update();
        } else {
            // Not focused, or the headset is showing a system overlay: nothing to draw.
            usleep(10000);
        }
        xr::endFrame();

        if (app->wantToQuit()) break;
    }

    delete app;
    vr::renderShutdown();
    xr::shutdown();
    env->DeleteGlobalRef(activity);
    vm->DetachCurrentThread();
}
