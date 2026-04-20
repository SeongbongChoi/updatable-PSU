#pragma once
// Fast Enhanced Private Set Union in the Balanced and Unbalanced Scenarios
// TBZ+25 - USENIX Security 2025

#include "volePSI/Defines.h"
#include "volePSI/config.h"

#ifdef VOLE_PSI_ENABLE_CPSI

#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Common/CuckooIndex.h"

#include "volePSI/pnmcrg/pnMCRG.h"

#include <vector>
#include <algorithm>


namespace volePSI
{
    namespace details
    {
        struct PsuTBZ25Base
        {
            u64 mSenderSize = 0;
            u64 mReceiverSize = 0;
            u64 mSsp = 40;
            u64 mNumThreads = 1;
            
            oc::PRNG mPrng;
            
            bool mInit = false;

            block mDummyValue = oc::ZeroBlock;

            void init(
                u64 senderSize,
                u64 receiverSize,
                u64 statSecParam,
                block seed,
                u64 numThreads);
        };
    }

    class PsuTBZ25Sender : public details::PsuTBZ25Base, public oc::TimerAdapter
    {
    public:
        std::vector<block> X;

        Proto send(span<block> inputs, Socket& chl);
    };

    class PsuTBZ25Receiver : public details::PsuTBZ25Base, public oc::TimerAdapter
    {
    public:
        std::vector<block> Y;
        std::vector<block> U;

        Proto recv(span<block> inputs, Socket& chl);
    };
}

#endif