#include "vr_input.h"

#include "vr_math.h"
#include "vr_render.h"

#include "../input/Keyboard.h"
#include "../input/Mouse.h"
#include "../time.h"

#include "../../NinecraftApp.h"
#include "../../client/Minecraft.h"
#include "../../client/renderer/GameRenderer.h"
#include "../../client/gui/VrKeyboard.h"
#include "vr_text.h"
#include "../../client/player/LocalPlayer.h"
#include "../../client/player/input/IBuildInput.h"
#include "../../client/player/input/IInputHolder.h"
#include "../../client/player/input/IMoveInput.h"
#include "../../client/player/input/ITurnInput.h"
#include "../../client/gui/Screen.h"
#include "../../client/gui/screens/ScreenChooser.h"
#include "../../world/entity/player/Inventory.h"
#include "../../world/entity/player/Player.h"
#include "../../world/item/crafting/Recipe.h"

namespace vr {
namespace {

// Stick and trigger thresholds.
const float kStickDeadZone = 0.2f;
const float kTriggerPressed = 0.5f;
const float kHotbarFlick = 0.7f;
// Smooth turning speed on the right stick, degrees per second at full deflection.
const float kTurnSpeed = 120.0f;
// Snap turning, the default for comfort: each flick of the right stick turns the view
// by a fixed angle. Set to false for smooth turning at kTurnSpeed.
const bool kSnapTurn = true;
const float kSnapDegrees = 30.0f;
const float kSnapFlick = 0.7f;   // stick deflection that counts as a flick
const float kSnapRearm = 0.3f;   // the stick must return below this before the next flick

float deadZone(float v) {
    float a = v < 0 ? -v : v;
    if (a < kStickDeadZone) return 0.0f;
    // Rescale so the output starts from zero at the edge of the dead zone.
    float out = (a - kStickDeadZone) / (1.0f - kStickDeadZone);
    return v < 0 ? -out : out;
}

// Movement from the left stick, jumping and flight from the right A/B.
class VrMoveInput : public IMoveInput {
public:
    VrMoveInput() : _wasCraft(false) {}

    void tick(Player* player) {
        State& st = state();
        const Hand& left = st.hands[kLeft];
        const Hand& right = st.hands[kRight];

        xa = -deadZone(left.stickX);   // stick right moves the player right (game +x is left)
        ya = deadZone(left.stickY);
        if (!st.running) {
            xa = ya = 0;
        }

        bool flying = player && player->abilities.flying;
        // A sets the jump flag in both modes, as space does on desktop: a double-tap toggles
        // flight, and while flying A also ascends (wantUp).
        jumping = right.buttonA;
        wantUp = flying && right.buttonA;
        wantDown = flying && right.buttonB;
        sneaking = left.stickClick || (!flying && right.buttonB);
        if (sneaking) {
            xa *= 0.3f;
            ya *= 0.3f;
        }

        // X on the left controller opens the 2x2 crafting grid on the press.
        bool craft = st.hands[kLeft].buttonX;
        if (craft && !_wasCraft && player) {
            player->startCrafting((int)player->x, (int)player->y, (int)player->z, Recipe::SIZE_2X2);
        }
        _wasCraft = craft;
    }

private:
    bool _wasCraft;
};

// Smooth turning is applied in syncPlayerToHead, so the game's turn input stays idle.
class VrTurnInput : public ITurnInput {
public:
    TurnDelta getTurnDelta() { return TurnDelta(0, 0); }
};

// Right trigger holds break and attack, right grip places or uses the held item
// (repeating while held), and the right stick flicks through the hotbar.
class VrBuildInput : public IBuildInput {
public:
    VrBuildInput()
    :   _buildHoldTicks(0),
        _wasGrip(false),
        _wasFlick(false)
    {}

    bool tickBuild(Player* player, BuildActionIntention* bai) {
        State& st = state();
        const Hand& right = st.hands[kRight];

        if (player && player->inventory) {
            float flick = right.stickY;
            bool flickUp = flick > kHotbarFlick;
            bool flickDown = flick < -kHotbarFlick;
            if ((flickUp || flickDown) && !_wasFlick) {
                int count = Inventory::MAX_SELECTION_SIZE;
                int slot = player->inventory->selected + (flickUp ? -1 : 1);
                slot = (slot % count + count) % count;
                player->inventory->selectSlot(slot);
            }
            _wasFlick = flickUp || flickDown;
        }

        if (right.trigger > kTriggerPressed) {
            *bai = BuildActionIntention(BuildActionIntention::BAI_REMOVE | BuildActionIntention::BAI_ATTACK);
            _wasGrip = false;
            _buildHoldTicks = 0;
            return true;
        }

        // Grip places or uses on the press, then repeats while it stays held.
        bool grip = right.grip > kTriggerPressed;
        bool emit = false;
        if (grip) {
            if (!_wasGrip) {
                emit = true;
                _buildHoldTicks = 0;
            } else if (++_buildHoldTicks >= kBuildDelayTicks) {
                emit = true;
                _buildHoldTicks = 0;
            }
        } else {
            _buildHoldTicks = 0;
        }
        _wasGrip = grip;

        if (emit) {
            *bai = BuildActionIntention(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
            return true;
        }
        return false;
    }

private:
    static const int kBuildDelayTicks = 10;
    int _buildHoldTicks;
    bool _wasGrip;
    bool _wasFlick;
};

// Edge state for the menu button, so a press and release each fire once.
bool gMenuWasDown = false;

// Trigger state of the last GUI click, so press and release are reported once.
bool gGuiTriggerWasDown = false;

// True while the press that started on the on-screen keyboard is held, so its
// release isn't sent to the screen underneath.
bool gPressOnKeyboard = false;

// True while a press has been sent to the screen as a mouse button down, so the
// matching release is always sent too.
bool gMouseDownSent = false;

// Right stick click state, so a click toggles the keyboard once.
bool gStickClickWasDown = false;

// The screen the panel was last set up for, so the panel is anchored once per screen.
const void* gPanelScreen = NULL;

// Y button state, so a press opens the inventory once.
bool gYWasDown = false;

}  // namespace

IInputHolder* createInputHolder() {
    return new CustomInputHolder(new VrMoveInput(), new VrTurnInput(), new VrBuildInput());
}

void updateInput(NinecraftApp* app, Minecraft* mc) {
    State& st = state();
    if (!isActive()) return;

    // Controller ray on the GUI panel becomes mouse input while a screen is open.
    // In game the trigger breaks blocks through the build input instead.
    // Menus stay put in the room, where the player was facing when they opened; the
    // in-game HUD follows the head.
    if (mc->screen != gPanelScreen) {
        gPanelScreen = mc->screen;
        if (mc->screen) anchorPanelToView();
        else headLockPanel();
    }

    float guiX = 0, guiY = 0;
    bool hit = aimAtPanel(&guiX, &guiY);
    bool trigger = st.hands[kRight].trigger > kTriggerPressed;
    const bool pressEdge = trigger && !gGuiTriggerWasDown;
    const bool releaseEdge = !trigger && gGuiTriggerWasDown;

    // Presses on the on-screen keyboard type into it; the screen never sees them.
    if (pressEdge) {
        VrKeyboard& keyboard = mc->gameRenderer->keyboard();
        gPressOnKeyboard = hit && keyboard.visible(mc) &&
            keyboard.pointerPressed(mc, (int)guiX, (int)guiY);
    }
    if (mc->screen && hit) {
        Mouse::feed(MouseAction::ACTION_MOVE, 0, (short)guiX, (short)guiY);
    }
    if (pressEdge && !gPressOnKeyboard && mc->screen && hit) {
        Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN, (short)guiX, (short)guiY);
        gMouseDownSent = true;
    }
    // A release always closes a press the screen was sent, even if the ray has left the
    // panel or the screen has closed, so the mouse button can't stay latched down.
    if (releaseEdge && gMouseDownSent) {
        Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP, (short)guiX, (short)guiY);
        gMouseDownSent = false;
    }
    if (releaseEdge) gPressOnKeyboard = false;
    gGuiTriggerWasDown = trigger;

    // The keyboard belongs to the screen that asked for it: it closes with that screen.
    if (!mc->screen && !vr::text::open() && mc->platform()->isKeyboardVisible()) {
        mc->platform()->hideKeyboard();
    }
    // Right stick click shows or hides the on-screen keyboard while a screen is open.
    bool stickClick = st.hands[kRight].stickClick;
    if (stickClick && !gStickClickWasDown && mc->screen && !vr::text::open()) {
        if (mc->platform()->isKeyboardVisible()) mc->platform()->hideKeyboard();
        else mc->platform()->showKeyboard();
    }
    gStickClickWasDown = stickClick;

    // Holding the grip keeps the use key held, so charged items (the bow, eating)
    // release when the grip is let go.
    Keyboard::setHeld(mc->options.keyUse.key,
        st.hands[kRight].grip > kTriggerPressed ? KeyboardAction::KEYDOWN : KeyboardAction::KEYUP);

    // Y on the left controller opens block selection (the inventory), as the "More..."
    // slot in the hotbar does. Only from play, not over another screen.
    bool yDown = st.hands[kLeft].buttonY;
    if (yDown && !gYWasDown && !mc->screen) {
        mc->screenChooser.setScreen(SCREEN_BLOCKSELECTION);
    }
    gYWasDown = yDown;

    // Menu button acts as the Android back button: pause in game, close screens.
    bool menu = st.hands[kLeft].menu;
    if (menu != gMenuWasDown) {
        app->handleBack(menu);
        gMenuWasDown = menu;
    }
}

void syncPlayerToHead(LocalPlayer* player) {
    State& st = state();
    if (!isActive() || !st.poseValid || !player) return;

    static float lastTime = -1.0f;
    float now = getTimeS();
    float dt = lastTime < 0.0f ? 0.0f : now - lastTime;
    lastTime = now;
    if (dt > 0.1f) dt = 0.1f;

    Vec3 headForward = forward(st.headOrientation);
    if (!st.bodyYawAligned) {
        // Start facing where the game already faces, so turning the headset to the
        // room's forward direction doesn't snap the view.
        st.bodyYaw = player->yRot - yawOf(headForward);
        st.bodyYawAligned = true;
        // Standing pose at the start of play is the zero point for physical movement.
        st.calibrationHead = st.headPosition;
        st.headCalibrated = true;
    }

    const float stick = st.hands[kRight].stickX;
    if (kSnapTurn) {
        // One turn per flick: the stick has to come back to centre before the next.
        static bool snapArmed = true;
        if (snapArmed && stick > kSnapFlick) {
            st.bodyYaw += kSnapDegrees;
            snapArmed = false;
        } else if (snapArmed && stick < -kSnapFlick) {
            st.bodyYaw -= kSnapDegrees;
            snapArmed = false;
        } else if (stick < kSnapRearm && stick > -kSnapRearm) {
            snapArmed = true;
        }
    } else {
        st.bodyYaw += deadZone(stick) * kTurnSpeed * dt;
    }

    Vec3 worldForward = rotateYaw(headForward, st.bodyYaw);
    player->yRot = yawOf(worldForward);
    player->xRot = pitchOf(worldForward);
}

}  // namespace vr
