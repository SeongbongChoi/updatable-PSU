#pragma once
// Updatable Private Set Union

#include "volePSI/Defines.h"
#include "volePSI/config.h"

#ifdef VOLE_PSI_ENABLE_CPSI

#include "volePSI/Psu_KLS26.h"

#include <vector>

namespace volePSI
{
    class UPsuSender : public PsuKLS26Sender
    {
    public:
        u64 mUpdateThreshold = 0;

        std::vector<block> Xp;

        void init(
            u64 senderSize,
            u64 receiverSize,
            u64 updateThreshold,
            u64 valueByteLength,
            u64 statSecParam,
            block seed,
            u64 numThreads,
            ValueShareType type = ValueShareType::Xor);

        Proto usend(span<block> Xadd, span<block> Xdel, Socket& chl);
    };

    class UPsuReceiver : public PsuKLS26Receiver
    {
    public:
        u64 mUpdateThreshold = 0;
        std::vector<block> Yp;
        std::vector<block> Uadd;
        std::vector<block> Udel;

        void init(
            u64 senderSize,
            u64 receiverSize,
            u64 updateThreshold,
            u64 valueByteLength,
            u64 statSecParam,
            block seed,
            u64 numThreads,
            ValueShareType type = ValueShareType::Xor);

        Proto urecv(span<block> Yadd, span<block> Ydel, Socket& chl);
    };
}

#endif
