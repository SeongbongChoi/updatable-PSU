#include "RsPsi_Tests.h"
#include "volePSI/RsPsi.h"
#include "volePSI/RsCpsi.h"
#include "coproto/Socket/AsioSocket.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Session.h"
#include "cryptoTools/Network/IOService.h"
#include "Common.h"
using namespace oc;
using namespace volePSI;
using coproto::LocalAsyncSocket;

namespace
{
    std::vector<u64> run(PRNG& prng, std::vector<block>& recvSet, std::vector<block> &sendSet, bool mal, u64 nt = 1, bool reduced = false)
    {
        auto sockets = LocalAsyncSocket::makePair();

        RsPsiReceiver recver;
        RsPsiSender sender;

        recver.init(sendSet.size(), recvSet.size(), 40, prng.get(), mal, nt, reduced);
        sender.init(sendSet.size(), recvSet.size(), 40, prng.get(), mal, nt, reduced);

        auto p0 = recver.run(recvSet, sockets[0]); 
        auto p1 = sender.run(sendSet, sockets[1]);

        eval(p0, p1);
        
        return recver.mIntersection;
    }


}

void Psi_Rs_empty_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    auto inter = run(prng, recvSet, sendSet, false);

    if (inter.size())
        throw RTE_LOC;
}


void Psi_Rs_partial_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
    {
        if (prng.getBit())
        {
            recvSet[i] = sendSet[(i + 312) % n];
            exp.insert(i);
        }
    }

    auto inter = run(prng, recvSet, sendSet, false);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
    {
        std::cout << "exp size " << exp.size() << std::endl;
        std::cout << "act size " << act.size() << std::endl;
        throw RTE_LOC;
    }
}


void Psi_Rs_full_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = run(prng, recvSet, sendSet, false);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}



void Psi_Rs_reduced_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = run(prng, recvSet, sendSet, false, 1, true);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}


void Psi_Rs_multiThrd_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    u64 nt = cmd.getOr("nt", 8);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    sendSet = recvSet;

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
        exp.insert(i);

    auto inter = run(prng, recvSet, sendSet, false, nt);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}


void Psi_Rs_mal_test(const CLP& cmd)
{
    u64 n = cmd.getOr("n", 13243);
    std::vector<block> recvSet(n), sendSet(n);
    PRNG prng(ZeroBlock);
    prng.get(recvSet.data(), recvSet.size());
    prng.get(sendSet.data(), sendSet.size());

    std::set<u64> exp;
    for (u64 i = 0; i < n; ++i)
    {
        if (prng.getBit())
        {
            recvSet[i] = sendSet[(i + 312) % n];
            exp.insert(i);
        }
    }

    auto inter = run(prng, recvSet, sendSet, true);
    std::set<u64> act(inter.begin(), inter.end());
    if (act != exp)
        throw RTE_LOC;
}

// ---------------------------------------------------------------------------
// RR22 (Rindal-Raghuraman, CCS'22) PSI performance baseline. Output format
// matches Psu_KLS26_perf_test so run_updatable.sh parses
// it unchanged.
// ---------------------------------------------------------------------------
void Psi_RR22_perf_test(const oc::CLP &cmd)
{
    u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 12));
    u64 m = cmd.getOr("m", 1ull << cmd.getOr("mm", 12));
    u64 nt = cmd.getOr("nt", 1);
    bool mal = cmd.isSet("malicious");

    // Half the receiver's set is shared with the sender, so the intersection is
    // non-trivial without changing the cost (which depends only on the sizes).
    std::vector<block> sendSet(n), recvSet(m);
    PRNG prng(oc::toBlock(123456));
    prng.get(sendSet.data(), sendSet.size());
    prng.get(recvSet.data(), recvSet.size());

    u64 expInter = std::min<u64>(n, m) / 2;
    for (u64 i = 0; i < expInter; ++i)
        recvSet[i] = sendSet[i];

    Timer timer, s, r;
    auto sockets = coproto::AsioSocket::makePair();

    RsPsiSender sender;
    RsPsiReceiver recver;

    sender.init(n, m, 40, prng.get(), mal, nt);
    recver.init(n, m, 40, prng.get(), mal, nt);

    sender.setTimer(s);
    recver.setTimer(r);

    timer.setTimePoint("start");
    s.setTimePoint("start");
    r.setTimePoint("start");

    auto p0 = recver.run(recvSet, sockets[0]);
    auto p1 = sender.run(sendSet, sockets[1]);

    eval(p0, p1);

    timer.setTimePoint("end");

    u64 total_bytes = sockets[0].bytesSent() + sockets[1].bytesSent();
    bool ok = (recver.mIntersection.size() == expInter);

    std::cout << std::endl;
    std::cout << "PSI Performance Test Results (RR22)" << std::endl;
    std::cout << "Sender set size:   " << n << std::endl;
    std::cout << "Receiver set size: " << m << std::endl;
    std::cout << "Intersection: expected " << expInter << " | actual "
              << recver.mIntersection.size() << " | " << (ok ? "PASS" : "FAIL")
              << std::endl;
    std::cout << "Total Comm = " << (double)total_bytes / (1 << 20) << " MB ("
              << (double)total_bytes / (1 << 10) << " KB)" << std::endl;
    std::cout << "Sender Comm = " << (double)sockets[1].bytesSent() / (1 << 20) << " MB ("
              << (double)sockets[1].bytesSent() / (1 << 10) << " KB)" << std::endl;
    std::cout << "Receiver Comm = " << (double)sockets[0].bytesSent() / (1 << 20) << " MB ("
              << (double)sockets[0].bytesSent() / (1 << 10) << " KB)" << std::endl;
    std::cout << "Total Time: " << timer << std::endl;
    std::cout << "Sender Timer:\n" << s << std::endl;
    std::cout << "Receiver Timer:\n" << r << std::endl;

    if (!ok)
        throw RTE_LOC;
}
