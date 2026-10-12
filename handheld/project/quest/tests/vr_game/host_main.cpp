// Runs the real game (NinecraftApp) on the desktop against the OpenXR runtime (Monado,
// headless). It creates a world the way the RPi build does, then plays it for a few
// hundred frames through the VR path, with synthetic controller input: walk, turn, break
// and place. It exits non-zero if the session never runs or the level never loads.
#include "AppPlatform.h"
#include "NinecraftApp.h"
#include "client/renderer/GameRenderer.h"
#include "client/gui/VrKeyboard.h"
#include "client/gui/screens/DialogDefinitions.h"
#include "platform/vr/vr_text.h"
#include "platform/input/Keyboard.h"
#include "host_platform.h"
#include "platform/gles1_shim.h"
#include "platform/vr/vr_render.h"
#include "platform/vr/vr_state.h"
#include "platform/vr/xr_session.h"
#include "client/player/LocalPlayer.h"
#include "world/level/LevelSettings.h"
#include "world/entity/player/Player.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#ifndef MCPE_ASSETS_DIR
#define MCPE_ASSETS_DIR "assets"
#endif
#ifndef MCPE_DATA_DIR
#define MCPE_DATA_DIR "/tmp/vr-game-data"
#endif

namespace {

const int kGuiW = 1280, kGuiH = 720;
const int kPlayFrames = 600;
// Level generation runs on a thread and is slow on a software GL host; allow a long wait.
const int kMaxFrames = 30000;

// A scripted player: walk forward, turn slowly, and alternate breaking and placing.
void scriptControllers(int frame) {
    vr::State& st = vr::state();
    vr::Hand& left = st.hands[vr::kLeft];
    vr::Hand& right = st.hands[vr::kRight];
    left.active = right.active = true;

    left.stickX = 0;
    left.stickY = (frame % 300) < 240 ? 1.0f : 0.0f;   // walk forward most of the time
    right.stickX = (frame % 200) < 100 ? 0.4f : -0.4f; // turn back and forth
    right.stickY = 0;

    right.trigger = (frame % 90) < 45 ? 1.0f : 0.0f;   // break blocks in bursts
    right.grip = (frame % 90) >= 60 && (frame % 90) < 75 ? 1.0f : 0.0f;  // place or use
    left.buttonY = right.buttonA = false;
}

// Presses the space bar on the VR keyboard and checks that a space reaches the text
// queue, which is where sign editing reads its characters. The coordinates are GUI
// canvas pixels: the bottom row's centre key, on a 1280x720 canvas.
bool pressKey(Minecraft* mc, int action) {
    int x = 0, y = 0;
    if (!mc->gameRenderer->keyboard().centreOf(mc, action, &x, &y)) return false;
    return mc->gameRenderer->keyboard().pointerPressed(mc, x, y);
}

bool exerciseKeyboard(Minecraft* mc, AppPlatform& platform) {
    platform.showKeyboard();
    const bool consumed = pressKey(mc, ' ');
    const bool typed = Keyboard::nextTextChar() && Keyboard::getChar() == ' ';
    platform.hideKeyboard();
    printf("keyboard: space pressed=%d typed=%d\n", consumed, typed);
    return consumed && typed;
}

// Asks for a world name through the platform, types "q" on the headset keyboard,
// then presses enter past the name and past the (empty) seed.
bool exerciseTextRequest(Minecraft* mc, AppPlatform& platform) {
    platform.createUserInput(DialogDefinitions::DIALOG_CREATE_NEW_WORLD);
    const bool pending = platform.getUserInputStatus() == -1;
    pressKey(mc, 'q');
    pressKey(mc, VrKeyboard::kActionEnter);
    pressKey(mc, VrKeyboard::kActionEnter);
    const int status = platform.getUserInputStatus();
    const StringVector v = platform.getUserInput();
    const bool ok = pending && status == 1 && v.size() == 2 && v[0] == "q" && v[1].empty();
    printf("text request: pending=%d status=%d name='%s' seed='%s' ok=%d\n", pending, status,
           v.size() > 0 ? v[0].c_str() : "", v.size() > 1 ? v[1].c_str() : "", ok);
    return ok;
}

}  // namespace

int main() {
    if (!xr::init(NULL, NULL)) {
        fprintf(stderr, "xr::init failed\n");
        return 2;
    }
    gles1ShimInit();
    vr::renderInit(kGuiW, kGuiH);

    AppPlatform_host platform(MCPE_ASSETS_DIR);
    NinecraftApp* app = new NinecraftApp();
    app->externalStoragePath = MCPE_DATA_DIR;
    app->externalCacheStoragePath = MCPE_DATA_DIR;

    AppContext context;
    context.doRender = false;
    context.platform = &platform;
    static_cast<App*>(app)->init(context);
    app->setSize(kGuiW, kGuiH);

    // A new world, created the way SelectWorldScreen does on the headset.
    LevelSettings settings(12345, GameType::Creative);
    app->selectLevel("vr_test", "vr_test", settings);
    app->hostMultiplayer();

    int framesRun = 0, framesPlayed = 0;
    bool levelReady = false;
    bool keyboardOk = false;
    for (int frame = 0; frame < kMaxFrames && framesPlayed < kPlayFrames && !xr::shouldQuit(); ++frame) {
        xr::pollEvents();
        xr::beginFrame();
        ++framesRun;
        if (framesRun % 500 == 0) {
            printf("progress frame=%d generating=%d status=%s\n", framesRun,
                   app->isLevelGenerated() ? 0 : 1, app->getProgressMessage());
            fflush(stdout);
        }

        if (vr::isActive() && xr::shouldRender() && vr::state().poseValid) {
            if (app->isLevelGenerated() && app->player) {
                if (!levelReady) {
                    levelReady = true;
                    printf("level ready after %d frames; player at %.1f %.1f %.1f\n", framesRun,
                           app->player->x, app->player->y, app->player->z);
                    keyboardOk = exerciseKeyboard(app, platform) && exerciseTextRequest(app, platform);
                }
                scriptControllers(framesPlayed);
                ++framesPlayed;
            }
            app->update();
        } else {
            usleep(10000);
        }
        xr::endFrame();
    }

    if (app->player) {
        printf("played %d frames; player at %.1f %.1f %.1f yRot=%.1f xRot=%.1f health=%d\n", framesPlayed,
               app->player->x, app->player->y, app->player->z, app->player->yRot, app->player->xRot,
               app->player->health);
    }
    printf("frames run=%d level=%s\n", framesRun, levelReady ? "ready" : "not ready");

    delete app;
    vr::renderShutdown();
    xr::shutdown();
    const bool ok = levelReady && framesPlayed >= kPlayFrames && keyboardOk;
    printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
