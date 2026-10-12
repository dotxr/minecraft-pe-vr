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
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unwind.h>

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

// Crash reports go to crash.txt in the app's external files folder. A crash on the
// headset leaves nothing else readable without a debugger, so the handler writes the
// signal, the fault address and a backtrace there, then lets the system handle the crash.
static char gCrashPath[512] = "";

struct BacktraceState {
    int count;
    void** frames;
    int max;
};

static _Unwind_Reason_Code captureFrame(struct _Unwind_Context* ctx, void* arg) {
    BacktraceState* st = (BacktraceState*) arg;
    if (st->count >= st->max) return _URC_END_OF_STACK;
    const uintptr_t ip = _Unwind_GetIP(ctx);
    if (ip) st->frames[st->count++] = (void*) ip;
    return _URC_NO_REASON;
}

static void writeText(int fd, const char* text) {
    write(fd, text, strlen(text));
}

static void writeHex(int fd, uintptr_t value) {
    char buf[24];
    char* p = buf + sizeof(buf);
    *--p = '\0';
    do {
        *--p = "0123456789abcdef"[value & 15];
        value >>= 4;
    } while (value);
    writeText(fd, "0x");
    writeText(fd, p);
}

static void crashHandler(int sig, siginfo_t* info, void* context) {
    const int fd = open(gCrashPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    writeText(fd, "fatal signal ");
    writeHex(fd, (uintptr_t) sig);
    writeText(fd, " at fault address ");
    writeHex(fd, (uintptr_t) info->si_addr);
    writeText(fd, "\n");

    void* frames[64];
    BacktraceState st = {0, frames, 64};
    _Unwind_Backtrace(captureFrame, &st);
    for (int i = 0; i < st.count; ++i) {
        Dl_info dl;
        writeHex(fd, (uintptr_t) st.frames[i]);
        if (dladdr(st.frames[i], &dl) && dl.dli_fname) {
            writeText(fd, " ");
            writeText(fd, dl.dli_fname);
            if (dl.dli_sname) {
                writeText(fd, " ");
                writeText(fd, dl.dli_sname);
            }
        }
        writeText(fd, "\n");
    }
    close(fd);

    LOGE("fatal signal %d; crash report written to %s", sig, gCrashPath);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void installCrashHandler(const std::string& externalPath) {
    snprintf(gCrashPath, sizeof(gCrashPath), "%s/crash.txt", externalPath.c_str());

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
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
    installCrashHandler(app->externalStoragePath);

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
