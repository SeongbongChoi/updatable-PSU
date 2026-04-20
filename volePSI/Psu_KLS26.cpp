#include "Psu_KLS26.h"

namespace volePSI
{
    void details::PsuKLS26Base::init(
        u64 senderSize,
        u64 receiverSize,
        u64 valueByteLength,
        u64 statSecParam,
        block seed,
        u64 numThreads,
        ValueShareType type)
    {
        mSenderSize = senderSize;
        mReceiverSize = receiverSize;
        mValueByteLength = valueByteLength;
        mSsp = statSecParam;
        mPrng.SetSeed(seed);
        mNumThreads = numThreads;
        mType = type;
        mInit = true;
    }

    Proto PsuKLS26Sender::send(span<block> inputs, Socket &chl)
    {
        setTimePoint("PsuKLS26Sender::send begin");

        if (!mInit)
            throw RTE_LOC;

        X.assign(inputs.begin(), inputs.end());

        RsOprfReceiver oprfReceiver;
        oprfReceiver.mSsp = mSsp;

        std::vector<block> oprfX(X.size());
        co_await oprfReceiver.receive(X, oprfX, mPrng, chl);

        setTimePoint("PsuKLS26Sender::send OPRF done");

        RsCpsiReceiver cpsi;
        cpsi.init(mReceiverSize, mSenderSize, 0, mSsp, mPrng.get(), mNumThreads, mType);

        RsCpsiReceiver::Sharing cpsiResult;
        co_await cpsi.receive(oprfX, cpsiResult, chl);

        setTimePoint("PsuKLS26Sender::send CPSI done");

        PSSender psSender;
        psSender.init(cpsiResult.mFlagBits.size(), mNumThreads);

        oc::BitVector psShare;
        co_await psSender.runPermAndShare(psShare, mPrng, chl);

        std::vector<int> perm = psSender.getPerm();
        std::vector<int> invPerm = psSender.getInvPerm();

        setTimePoint("PsuKLS26Sender::send PS done");

        std::vector<u64> idxShareToSet(perm.size(), UINT64_MAX);

        for (u64 i = 0; i < X.size(); ++i)
        {
            idxShareToSet[invPerm[cpsiResult.mMapping[i]]] = i;
        }

        std::vector<std::array<block, 2>> otMessages(perm.size());

        for (u64 i = 0; i < perm.size(); ++i)
        {
            psShare[i] = psShare[i] ^ cpsiResult.mFlagBits[perm[i]];

            otMessages[i][!psShare[i]] = mDummyValue;
            if (idxShareToSet[i] != UINT64_MAX)
                otMessages[i][psShare[i]] = X[idxShareToSet[i]];
            else
                otMessages[i][psShare[i]] = mDummyValue;
        }

        oc::SilentOtExtSender otSender;
        oc::CuckooParam params = oc::CuckooIndex<>::selectParams(mReceiverSize, mSsp, 0, 3);
        u64 estimatedBins = params.numBins();
        otSender.configure(estimatedBins, 2, mNumThreads);
        co_await otSender.sendChosen(otMessages, mPrng, chl);

        setTimePoint("PsuKLS26Sender::send OT done");
        setTimePoint("PsuKLS26Sender::send done");
    }

    Proto PsuKLS26Receiver::recv(span<block> inputs, Socket &chl)
    {
        setTimePoint("PsuKLS26Receiver::recv begin");

        if (!mInit)
            throw RTE_LOC;

        Y.assign(inputs.begin(), inputs.end());

        RsOprfSender oprfSender;
        oprfSender.mSsp = mSsp;
        co_await oprfSender.send(mSenderSize, mPrng, chl);

        std::vector<block> oprfY(Y.size());
        oprfSender.eval(Y, oprfY);

        setTimePoint("PsuKLS26Receiver::recv OPRF done");

        RsCpsiSender cpsi;
        cpsi.init(mReceiverSize, mSenderSize, 0, mSsp, mPrng.get(), mNumThreads, mType);

        oc::Matrix<u8> dummyValues(oprfY.size(), 0);
        RsCpsiSender::Sharing cpsiResult;
        co_await cpsi.send(oprfY, dummyValues, cpsiResult, chl);

        setTimePoint("PsuKLS26Receiver::recv CPSI done");

        PSReceiver psReceiver;
        psReceiver.init(cpsiResult.mFlagBits.size(), mNumThreads);

        oc::BitVector psShare;
        co_await psReceiver.runPermAndShare(cpsiResult.mFlagBits, psShare, mPrng, chl);

        setTimePoint("PsuKLS26Receiver::recv PS done");

        std::vector<block> otResults(psShare.size());

        oc::SilentOtExtReceiver otReceiver;
        oc::CuckooParam params = oc::CuckooIndex<>::selectParams(mReceiverSize, mSsp, 0, 3);
        u64 estimatedBins = params.numBins();
        otReceiver.configure(estimatedBins, 2, mNumThreads);
        co_await otReceiver.receiveChosen(psShare, otResults, mPrng, chl);

        setTimePoint("PsuKLS26Receiver::recv OT done");

        U.clear();
        XminusY.clear();

        for (u64 i = 0; i < otResults.size(); ++i)
        {
            if (otResults[i] != mDummyValue)
            {
                XminusY.push_back(otResults[i]);
                U.push_back(otResults[i]);
            }
        }

        for (const block &y : Y)
        {
            U.push_back(y);
        }

        std::sort(U.begin(), U.end());
        U.erase(std::unique(U.begin(), U.end()), U.end());

        setTimePoint("PsuKLS26Receiver::recv union done");
        setTimePoint("PsuKLS26Receiver::recv done");
    }
}
