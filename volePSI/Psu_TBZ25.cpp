#include "Psu_TBZ25.h"

#ifdef VOLE_PSI_ENABLE_CPSI

namespace volePSI
{    
    void details::PsuTBZ25Base::init(
        u64 senderSize,
        u64 receiverSize,
        u64 statSecParam,
        block seed,
        u64 numThreads)
    {
        mSenderSize = senderSize;
        mReceiverSize = receiverSize;
        mSsp = statSecParam;
        mPrng.SetSeed(seed);
        mNumThreads = numThreads;
        mInit = true;
    }

    Proto PsuTBZ25Sender::send(span<block> inputs, Socket& chl)
    {
        setTimePoint("PsuTBZ25Sender::send begin");

        if (!mInit)
            throw RTE_LOC;

        if (mSenderSize != inputs.size())
            throw RTE_LOC;

        X.assign(inputs.begin(), inputs.end());

        oc::CuckooParam params = oc::CuckooIndex<>::selectParams(mSenderSize, mSsp, 0, 3);
        u32 numBins = params.numBins();

        std::vector<block> permutedX(numBins);
        std::vector<block> pnMCRG_out(numBins);
        std::vector<block> vecOTP_out(numBins);

        setTimePoint("PsuTBZ25Sender::send pnMCRG start");

        pnMCRG(0, mSenderSize, X, pnMCRG_out, permutedX, chl, mNumThreads);

        setTimePoint("PsuTBZ25Sender::send pnMCRG done");

        for (u32 i = 0; i < numBins; ++i)
        {
            vecOTP_out[i] = pnMCRG_out[i] ^ permutedX[i];
        }

        co_await chl.send(std::move(vecOTP_out));

        setTimePoint("PsuTBZ25Sender::send OTP sent");
        setTimePoint("PsuTBZ25Sender::send done");
    }

    Proto PsuTBZ25Receiver::recv(span<block> inputs, Socket& chl)
    {
        setTimePoint("PsuTBZ25Receiver::recv begin");

        if (!mInit)
            throw RTE_LOC;

        if (mReceiverSize != inputs.size())
            throw RTE_LOC;

        Y.assign(inputs.begin(), inputs.end());

        oc::CuckooParam params = oc::CuckooIndex<>::selectParams(mReceiverSize, mSsp, 0, 3);
        u32 numBins = params.numBins();

        std::vector<block> permutedY(numBins);
        std::vector<block> pnMCRG_out(numBins);
        std::vector<block> vecOTP_out(numBins);

        setTimePoint("PsuTBZ25Receiver::recv pnMCRG start");

        pnMCRG(1, mReceiverSize, Y, pnMCRG_out, permutedY, chl, mNumThreads);

        setTimePoint("PsuTBZ25Receiver::recv pnMCRG done");

        co_await chl.recv(vecOTP_out);

        setTimePoint("PsuTBZ25Receiver::recv OTP received");

        U.clear();
        U.reserve(2 * Y.size());
        U.assign(Y.begin(), Y.end());

        for (u32 i = 0; i < numBins; ++i)
        {
            block recovered = vecOTP_out[i] ^ pnMCRG_out[i];
            
            if (recovered.mData[0] == 1)
            {
                block element = oc::toBlock(0, recovered.mData[1]);
                U.push_back(element);
            }
        }

        std::sort(U.begin(), U.end(), 
            [](const block& a, const block& b) {
                if (a.mData[0] != b.mData[0]) 
                    return a.mData[0] < b.mData[0];
                return a.mData[1] < b.mData[1];
            });
        
        U.erase(
            std::unique(U.begin(), U.end(),
                [](const block& a, const block& b) {
                    return a.mData[0] == b.mData[0] && a.mData[1] == b.mData[1];
                }),
            U.end());

        setTimePoint("PsuTBZ25Receiver::recv union computed");
        setTimePoint("PsuTBZ25Receiver::recv done");
    }
}

#endif