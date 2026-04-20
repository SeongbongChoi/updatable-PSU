#include "UPsu.h"

namespace volePSI
{
    struct BlockHash
    {
        std::size_t operator()(const block &b) const
        {
            return std::hash<block>{}(b);
        }
    };

    void UPsuSender::init(
        u64 senderSize,
        u64 receiverSize,
        u64 updateThreshold,
        u64 valueByteLength,
        u64 statSecParam,
        block seed,
        u64 numThreads,
        ValueShareType type)
    {
        PsuKLS26Sender::init(senderSize, receiverSize, valueByteLength, statSecParam, seed, numThreads, type);
        mUpdateThreshold = updateThreshold;
    }

    Proto UPsuSender::usend(span<block> Xadd, span<block> Xdel, Socket &chl)
    {
        setTimePoint("UPsuSender::usend begin");

        Xp.clear();
        Xp.reserve(2 * mUpdateThreshold);
        Xp.insert(Xp.end(), Xadd.begin(), Xadd.end());
        Xp.insert(Xp.end(), Xdel.begin(), Xdel.end());
        setTimePoint("UPsuSender::usend Xp Computation Done");

        RsOprfReceiver uOprfReceiver;
        uOprfReceiver.mSsp = mSsp;
        std::vector<block> oprfXp(Xp.size());
        co_await uOprfReceiver.receive(Xp, oprfXp, mPrng, chl);

        setTimePoint("UPsuSender::usend OPRF done");

        RsCpsiReceiver uCpsi;
        uCpsi.init(mReceiverSize + mUpdateThreshold, 2 * mUpdateThreshold, 0, mSsp, mPrng.get(), mNumThreads, mType);

        RsCpsiReceiver::Sharing cpsiResult;
        co_await uCpsi.receive(oprfXp, cpsiResult, chl);

        setTimePoint("UPsuSender::usend CPSI done");

        u64 numBins = cpsiResult.mFlagBits.size();

        PSSender psSender;
        psSender.init(numBins, mNumThreads);

        oc::BitVector psShare;
        co_await psSender.runPermAndShare(psShare, mPrng, chl);

        std::vector<int> perm = psSender.getPerm();
        std::vector<int> invPerm = psSender.getInvPerm();

        setTimePoint("UPsuSender::usend PS done");

        std::vector<u64> idxShareToSet(perm.size(), UINT64_MAX);

        for (u64 i = 0; i < Xp.size(); ++i)
        {
            idxShareToSet[invPerm[cpsiResult.mMapping[i]]] = i;
        }

        std::vector<std::array<block, 2>> otMessages(perm.size());

        for (u64 i = 0; i < perm.size(); ++i)
        {
            u64 permutedIdx = perm[i];
            psShare[i] = psShare[i] ^ cpsiResult.mFlagBits[permutedIdx];

            otMessages[i][!psShare[i]] = mDummyValue;
            if (idxShareToSet[i] != UINT64_MAX)
            {
                u64 origIdx = idxShareToSet[i];
                block taggedValue = Xp[origIdx];
                taggedValue.set<u8>(15, origIdx >= Xadd.size() ? 1 : 0);
                otMessages[i][psShare[i]] = taggedValue;
            }
            else
            {
                otMessages[i][psShare[i]] = mDummyValue;
            }
        }

        oc::SilentOtExtSender updateOtSender;
        updateOtSender.configure(perm.size(), 2, mNumThreads);
        co_await updateOtSender.sendChosen(otMessages, mPrng, chl);

        setTimePoint("UPsuSender::usend OT done");
        setTimePoint("UPsuSender::usend Sender's Set Update done");

        std::unordered_set<block, BlockHash> XdelSet(Xdel.begin(), Xdel.end());
        std::vector<block> Xnew;
        Xnew.reserve(X.size());

        for (const block &item : X)
        {
            if (XdelSet.find(item) == XdelSet.end())
                Xnew.push_back(item);
        }
        Xnew.insert(Xnew.end(), Xadd.begin(), Xadd.end());
        X = std::move(Xnew);

        setTimePoint("UPsuSender::usend Xnew computation done");

        RsPsiSender psiSender;
        psiSender.init(mSenderSize, mUpdateThreshold, mSsp, mPrng.get(), false, mNumThreads);
        co_await psiSender.run(X, chl);

        setTimePoint("UPsuSender::usend PSI(Xnew, Ydel) done");
        setTimePoint("UPsuSender::usend Receiver's Set Update done");
        setTimePoint("UPsuSender::usend done");
    }

    void UPsuReceiver::init(
        u64 senderSize,
        u64 receiverSize,
        u64 updateThreshold,
        u64 valueByteLength,
        u64 statSecParam,
        block seed,
        u64 numThreads,
        ValueShareType type)
    {
        PsuKLS26Receiver::init(senderSize, receiverSize, valueByteLength, statSecParam, seed, numThreads, type);
        mUpdateThreshold = updateThreshold;
        Uadd.clear();
        Udel.clear();
    }

    Proto UPsuReceiver::urecv(span<block> Yadd, span<block> Ydel, Socket &chl)
    {
        setTimePoint("UPsuReceiver::urecv begin");

        // ===== 1. OPRF =====
        Yp.clear();
        Yp.reserve(Y.size() + mUpdateThreshold);
        Yp.insert(Yp.end(), Y.begin(), Y.end());
        Yp.insert(Yp.end(), Yadd.begin(), Yadd.end());

        RsOprfSender uOprfSender;
        uOprfSender.mSsp = mSsp;
        co_await uOprfSender.send(2 * mUpdateThreshold, mPrng, chl);

        std::vector<block> oprfYp(Yp.size());
        uOprfSender.eval(Yp, oprfYp);
        setTimePoint("UPsuReceiver::urecv OPRF done");

        // ===== 2. CPSI =====
        RsCpsiSender uCpsi;
        uCpsi.init(mReceiverSize + mUpdateThreshold, 2 * mUpdateThreshold, 0, mSsp, mPrng.get(), mNumThreads, mType);

        oc::Matrix<u8> dummyValues(oprfYp.size(), 0);
        RsCpsiSender::Sharing cpsiResult;
        co_await uCpsi.send(oprfYp, dummyValues, cpsiResult, chl);
        setTimePoint("UPsuReceiver::urecv CPSI done");

        // ===== 3. PS =====
        u64 numBins = cpsiResult.mFlagBits.size();

        PSReceiver psReceiver;
        psReceiver.init(numBins, mNumThreads);

        oc::BitVector psShare;
        co_await psReceiver.runPermAndShare(cpsiResult.mFlagBits, psShare, mPrng, chl);
        setTimePoint("UPsuReceiver::urecv PS done");

        // ===== 4. OT =====
        std::vector<block> otResults(psShare.size());

        oc::SilentOtExtReceiver updateOtReceiver;
        updateOtReceiver.configure(psShare.size(), 2, mNumThreads);
        co_await updateOtReceiver.receiveChosen(psShare, otResults, mPrng, chl);
        setTimePoint("UPsuReceiver::urecv OT done");

        for (u64 i = 0; i < otResults.size(); ++i)
        {
            if (otResults[i] != mDummyValue)
            {
                u8 tag = otResults[i].get<u8>(15);
                block value = otResults[i];
                value.set<u8>(15, 0);

                if (tag == 0)
                {
                    Uadd.push_back(value);
                }
                else if (tag == 1)
                {
                    Udel.push_back(value);
                }
            }
        }
        setTimePoint("UPsuReceiver::urecv Sender's Set Update done");

        // ===== 5. Local =====
        std::unordered_set<block, BlockHash> XminusYSet(XminusY.begin(), XminusY.end());

        for (u64 i = 0; i < Yadd.size(); ++i)
        {
            if (XminusYSet.find(Yadd[i]) == XminusYSet.end())
            {
                Uadd.push_back(Yadd[i]);
            }
        }
        setTimePoint("UPsuReceiver::urecv Local computation done");

        // ===== 6. PSI =====
        RsPsiReceiver psiReceiver;
        psiReceiver.init(mSenderSize, mUpdateThreshold, mSsp, mPrng.get(), false, mNumThreads);
        co_await psiReceiver.run(Ydel, chl);
        setTimePoint("UPsuReceiver::urecv PSI(Ydel, Xnew) done");

        std::unordered_set<u64> intersectionSet(
            psiReceiver.mIntersection.begin(),
            psiReceiver.mIntersection.end());

        for (u64 i = 0; i < Ydel.size(); ++i)
        {
            if (intersectionSet.find(i) == intersectionSet.end())
            {
                Udel.push_back(Ydel[i]);
            }
        }

        setTimePoint("UPsuReceiver::urecv Receiver's Set Update done");

        std::unordered_set<block, BlockHash> YdelSet(Ydel.begin(), Ydel.end());
        Y = Yp;
        Y.erase(
            std::remove_if(Y.begin(), Y.end(),
                           [&YdelSet](const block &b)
                           {
                               return YdelSet.find(b) != YdelSet.end();
                           }),
            Y.end());

        std::unordered_set<block, BlockHash> delSet(Udel.begin(), Udel.end());
        U.erase(
            std::remove_if(U.begin(), U.end(),
                           [&delSet](const block &b)
                           {
                               return delSet.find(b) != delSet.end();
                           }),
            U.end());

        U.insert(U.end(), Uadd.begin(), Uadd.end());

        setTimePoint("UPsuReceiver::urecv Union Update done");
        setTimePoint("UPsuReceiver::urecv done");
    }
}
