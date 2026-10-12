#ifndef PLATFORM_VR_VR_HOOKS_H__
#define PLATFORM_VR_VR_HOOKS_H__

// Switches the shared game code between the headset path and the other
// platforms. MCPE_VR is defined only by the Quest build
// (handheld/project/quest), so the iOS, Windows and Raspberry Pi builds never
// reference the VR layer and behave exactly as before.
#ifdef MCPE_VR
    #include "vr_state.h"
    #include "vr_render.h"
    #include "vr_input.h"
    #define MCPE_VR_ACTIVE (vr::isActive())
#else
    #define MCPE_VR_ACTIVE (false)
#endif

#endif /*PLATFORM_VR_VR_HOOKS_H__*/
