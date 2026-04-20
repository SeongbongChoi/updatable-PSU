

#include "cryptoTools/Common/Log.h"
#include <functional>
#include "UnitTests.h"

#include "Paxos_Tests.h"
#include "RsOprf_Tests.h"
#include "RsOpprf_Tests.h"
#include "RsPsi_Tests.h"
#include "Psu_KLS26_Tests.h"
#include "Psu_TBZ25_Tests.h"
#include "GMW_Tests.h"
#include "volePSI/GMW/Circuit.h"
#include "FileBase_Tests.h"

namespace volePSI_Tests
{
    using namespace volePSI;
    oc::TestCollection Tests([](oc::TestCollection& t) {
                                                   
#ifdef VOLE_PSI_ENABLE_CPSI
        // t.add("Cpsi_Rs_empty_test          ", Cpsi_Rs_empty_test);
        // t.add("Cpsi_Rs_partial_test        ", Cpsi_Rs_partial_test);
        // t.add("Cpsi_Rs_full_test           ", Cpsi_Rs_full_test);
        // t.add("Cpsi_Rs_full_asym_test      ", Cpsi_Rs_full_asym_test);
        // t.add("Cpsi_Rs_full_add32_test     ", Cpsi_Rs_full_add32_test);

        t.add("Psu_TBZ25_perf_test           ", Psu_TBZ25_perf_test);
        t.add("Psu_KLS26_perf_test           ", Psu_KLS26_perf_test);
        t.add("PSU_update_test               ", PSU_update_test);

#endif

        // t.add("filebase_readSet_Test       ", filebase_readSet_Test);
        // t.add("filebase_psi_bin_Test       ", filebase_psi_bin_Test);
        // t.add("filebase_psi_csv_Test       ", filebase_psi_csv_Test);
        // t.add("filebase_psi_csvh_Test      ", filebase_psi_csvh_Test);

    });
}
