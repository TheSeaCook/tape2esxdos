// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

#include "cpuspeed.h"

unsigned char t_states_to_mhz(unsigned int tstates) __z88dk_fastcall {
    if (tstates<3116) { // 3036/3080 48/128
        return CPU_3MHZ;
    } else if (tstates<6233) {  // 6163
        return CPU_7MHZ;
    } else if (tstates<12466) { // 9777
        return CPU_14MHZ;
    //} else if (tstates<24932) { // 19559
        //return CPU_28MHZ;
    } else {
        return CPU_28MHZ;
    }
}

// EOF vim: ts=4:sw=4:et:ai:
