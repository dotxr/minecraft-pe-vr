#include "vr_state.h"

namespace vr {

State& state() {
    static State s;
    return s;
}

}  // namespace vr
