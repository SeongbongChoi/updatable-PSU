#pragma once

#include "cryptoTools/Common/CLP.h"

namespace volePSI
{
    // Performance test for basic PSU (KLS26)
    void Psu_KLS26_perf_test(const oc::CLP& cmd);

    // Update test for updatable PSU (KLS26)
    void PSU_update_test(const oc::CLP& cmd);
}
