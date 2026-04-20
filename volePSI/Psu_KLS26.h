#pragma once
// Private Set Union
// KLS26 — ACM SAC 2026

#include "volePSI/Defines.h"
#include "volePSI/config.h"

#ifdef VOLE_PSI_ENABLE_CPSI

#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Common/Timer.h"
#include "volePSI/RsCpsi.h"
#include "volePSI/RsPsi.h"
#include "volePSI/PS/PSReceiver.h"
#include "volePSI/PS/PSSender.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"

#include <vector>

namespace volePSI
{
    namespace details
    {
        struct PsuKLS26Base
        {
            u64 mSenderSize = 0;
            u64 mReceiverSize = 0;
            u64 mValueByteLength = 0;
            u64 mSsp = 0;
            u64 mNumThreads = 0;

            oc::PRNG mPrng;
            ValueShareType mType = ValueShareType::Xor;

            bool mInit = false;

            block mDummyValue = oc::ZeroBlock;

            void init(
                u64 senderSize,
                u64 receiverSize,
                u64 valueByteLength,
                u64 statSecParam,
                block seed,
                u64 numThreads,
                ValueShareType type = ValueShareType::Xor);
        };
    }

    class PsuKLS26Sender : public details::PsuKLS26Base, public oc::TimerAdapter
    {
    public:
        std::vector<block> X;

        Proto send(span<block> inputs, Socket& chl);
    };

    class PsuKLS26Receiver : public details::PsuKLS26Base, public oc::TimerAdapter
    {
    public:
        std::vector<block> Y;
        std::vector<block> U;
        std::vector<block> XminusY;

        Proto recv(span<block> inputs, Socket& chl);
    };
}

#endif
