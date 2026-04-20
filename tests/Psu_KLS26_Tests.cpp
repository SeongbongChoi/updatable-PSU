#include "Psu_KLS26_Tests.h"
#include "volePSI/Psu_KLS26.h"
#include "volePSI/UPsu.h"
#include "coproto/Socket/AsioSocket.h"
#include "Common.h"

#include <set>
#include <algorithm>

using namespace std;
using namespace volePSI;
using namespace oc;
using coproto::AsioSocket;
using coproto::LocalAsyncSocket;

namespace volePSI
{
    void Psu_KLS26_perf_test(const oc::CLP &cmd)
    {
        u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 12));
        u64 m = cmd.getOr("m", 1ull << cmd.getOr("mm", 12));
        u64 nt = cmd.getOr("nt", 1);

        std::vector<block> sendSet(n);
        std::vector<block> recvSet(m);

        PRNG prng(oc::toBlock(123456));

        for (u64 i = 0; i < n; ++i)
            sendSet[i] = prng.get<block>();

        for (u64 i = 0; i < m; ++i)
            recvSet[i] = prng.get<block>();

        Timer timer, s, r;
        auto sockets = AsioSocket::makePair();

        PsuKLS26Sender sender;
        PsuKLS26Receiver recver;

        block commonSeed = oc::toBlock(42, 42);
        sender.init(n, m, 0, 40, commonSeed, nt);
        recver.init(n, m, 0, 40, commonSeed, nt);

        recver.setTimer(r);
        sender.setTimer(s);

        timer.setTimePoint("start");
        s.setTimePoint("start");
        r.setTimePoint("start");

        auto p0 = recver.recv(recvSet, sockets[0]);
        auto p1 = sender.send(sendSet, sockets[1]);

        eval(p0, p1);

        timer.setTimePoint("end");

        u64 unionSize = recver.U.size();
        u64 total_bytes = sockets[0].bytesSent() + sockets[1].bytesSent();

        std::cout << std::endl;
        std::cout << "PSU Performance Test Results (KLS26)" << std::endl;
        std::cout << "Set size: " << n << std::endl;
        std::cout << "Union size: " << unionSize << std::endl;
        std::cout << "Total Comm = " << (double)total_bytes / (1 << 20) << " MB ("
                  << (double)total_bytes / (1 << 10) << " KB)" << std::endl;
        std::cout << "Sender Comm = " << (double)sockets[1].bytesSent() / (1 << 20) << " MB ("
                  << (double)sockets[1].bytesSent() / (1 << 10) << " KB)" << std::endl;
        std::cout << "Receiver Comm = " << (double)sockets[0].bytesSent() / (1 << 20) << " MB ("
                  << (double)sockets[0].bytesSent() / (1 << 10) << " KB)" << std::endl;
        std::cout << "Total Time: " << timer << std::endl;
        std::cout << "Sender Timer:\n"
                  << s << std::endl;
        std::cout << "Receiver Timer:\n"
                  << r << std::endl;
    }

    void PSU_update_test(const oc::CLP &cmd)
    {
        u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 12));
        u64 t = cmd.getOr("t", 1ull << cmd.getOr("tt", 8));
        u64 nt = cmd.getOr("nt", 1);
        bool verbose = cmd.isSet("v");

        double ratio_old_intersection = 0.3;
        double ratio_add_intersection = 0.3;
        double ratio_del = 0.3;

        u64 intersect_old = std::max<u64>(1, static_cast<u64>(n * ratio_old_intersection));
        u64 intersect_add = std::max<u64>(1, static_cast<u64>(t * ratio_add_intersection));

        auto makeBlock = [](u64 idx)
        {
            block b = oc::toBlock(idx, 0);
            b.set<u8>(15, 0);
            return b;
        };

        u64 idx = 1;

        std::cout << "\n=== Test Configuration ===" << std::endl;
        std::cout << "Set size (n): " << n << std::endl;
        std::cout << "Update threshold (t): " << t << std::endl;
        std::cout << "Intersection ratio (old): " << (ratio_old_intersection * 100) << "%" << std::endl;
        std::cout << "Intersection ratio (add): " << (ratio_add_intersection * 100) << "%" << std::endl;
        std::cout << "Deletion ratio: " << (ratio_del * 100) << "%" << std::endl;

        std::vector<block> Xold(n);
        std::vector<block> Yold(n);

        for (u64 i = 0; i < intersect_old; ++i)
        {
            block b = makeBlock(idx++);
            Xold[i] = b;
            Yold[i] = b;
        }

        for (u64 i = intersect_old; i < n; ++i)
            Xold[i] = makeBlock(idx++);

        for (u64 i = intersect_old; i < n; ++i)
            Yold[i] = makeBlock(idx++);

        std::sort(Xold.begin(), Xold.end());
        std::sort(Yold.begin(), Yold.end());

        std::set<block> Xadd_set;
        std::set<block> Yadd_set;

        for (u64 i = 0; i < intersect_add; ++i)
        {
            block b = makeBlock(idx++);
            Xadd_set.insert(b);
            Yadd_set.insert(b);
        }

        while (Xadd_set.size() < t)
            Xadd_set.insert(makeBlock(idx++));

        while (Yadd_set.size() < t)
            Yadd_set.insert(makeBlock(idx++));

        std::vector<block> Xadd(Xadd_set.begin(), Xadd_set.end());
        std::vector<block> Yadd(Yadd_set.begin(), Yadd_set.end());

        u64 xdel_size = std::max<u64>(1, std::min<u64>(t, static_cast<u64>(n * ratio_del)));
        u64 ydel_size = std::max<u64>(1, std::min<u64>(t, static_cast<u64>(n * ratio_del)));

        std::set<block> Xdel_set;
        std::set<block> Ydel_set;

        std::vector<u64> xold_indices(n);
        std::iota(xold_indices.begin(), xold_indices.end(), 0);
        std::shuffle(xold_indices.begin(), xold_indices.end(), std::mt19937(12345));

        for (u64 i = 0; i < xdel_size; ++i)
            Xdel_set.insert(Xold[xold_indices[i]]);

        std::vector<u64> yold_indices(n);
        std::iota(yold_indices.begin(), yold_indices.end(), 0);
        std::shuffle(yold_indices.begin(), yold_indices.end(), std::mt19937(67890));

        for (u64 i = 0; i < ydel_size; ++i)
            Ydel_set.insert(Yold[yold_indices[i]]);

        std::vector<block> Xdel(Xdel_set.begin(), Xdel_set.end());
        std::vector<block> Ydel(Ydel_set.begin(), Ydel_set.end());

        std::set<block> expectedInitialUnion(Xold.begin(), Xold.end());
        expectedInitialUnion.insert(Yold.begin(), Yold.end());

        std::set<block> Xnew(Xold.begin(), Xold.end());
        std::set<block> Ynew(Yold.begin(), Yold.end());

        for (const auto &x : Xdel)
            Xnew.erase(x);
        for (const auto &y : Ydel)
            Ynew.erase(y);

        Xnew.insert(Xadd.begin(), Xadd.end());
        Ynew.insert(Yadd.begin(), Yadd.end());

        std::set<block> expectedFinalUnion(Xnew.begin(), Xnew.end());
        expectedFinalUnion.insert(Ynew.begin(), Ynew.end());

        std::cout << "\n=== Expected Union Sizes ===" << std::endl;
        std::cout << "Expected initial union size: " << expectedInitialUnion.size() << std::endl;
        std::cout << "Expected final union size: " << expectedFinalUnion.size() << std::endl;

        Timer timer, s, r;
        auto sockets = AsioSocket::makePair();

        UPsuSender sender;
        UPsuReceiver recver;

        block commonSeed = oc::toBlock(42, 42);
        sender.init(n, n, t, 0, 40, commonSeed, nt);
        recver.init(n, n, t, 0, 40, commonSeed, nt);

        recver.setTimer(r);
        sender.setTimer(s);

        timer.setTimePoint("init_start");
        s.setTimePoint("init_start");
        r.setTimePoint("init_start");

        auto p0_init = recver.recv(Yold, sockets[0]);
        auto p1_init = sender.send(Xold, sockets[1]);

        eval(p0_init, p1_init);
        timer.setTimePoint("init_end");

        u64 initialUnionSize = recver.U.size();
        u64 initial_bytes = sockets[0].bytesSent() + sockets[1].bytesSent();

        timer.setTimePoint("update_start");
        s.setTimePoint("update_start");
        r.setTimePoint("update_start");

        auto p0_update = recver.urecv(Yadd, Ydel, sockets[0]);
        auto p1_update = sender.usend(Xadd, Xdel, sockets[1]);
        eval(p0_update, p1_update);

        timer.setTimePoint("update_end");

        u64 finalUnionSize = recver.U.size();
        u64 total_bytes = sockets[0].bytesSent() + sockets[1].bytesSent();
        u64 update_bytes = total_bytes - initial_bytes;

        bool initialUnionCorrect = (initialUnionSize == expectedInitialUnion.size());
        bool finalUnionCorrect = (finalUnionSize == expectedFinalUnion.size());

        std::set<block> actualFinalUnion(recver.U.begin(), recver.U.end());

        std::vector<block> inExpectedNotActual;
        std::vector<block> inActualNotExpected;

        std::set_difference(
            expectedFinalUnion.begin(), expectedFinalUnion.end(),
            actualFinalUnion.begin(), actualFinalUnion.end(),
            std::back_inserter(inExpectedNotActual));

        std::set_difference(
            actualFinalUnion.begin(), actualFinalUnion.end(),
            expectedFinalUnion.begin(), expectedFinalUnion.end(),
            std::back_inserter(inActualNotExpected));

        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "\n=== Union Verification ===" << std::endl;
        std::cout << "Expected initial: " << expectedInitialUnion.size()
                  << " | Actual: " << initialUnionSize
                  << " | " << (initialUnionCorrect ? "PASS" : "FAIL") << std::endl;
        std::cout << "Expected final:   " << expectedFinalUnion.size()
                  << " | Actual: " << finalUnionSize
                  << " | " << (finalUnionCorrect ? "PASS" : "FAIL") << std::endl;

        if (!finalUnionCorrect)
        {
            std::cout << "\nDifference: " << (int64_t)finalUnionSize - (int64_t)expectedFinalUnion.size() << std::endl;
            std::cout << "Missing: " << inExpectedNotActual.size()
                      << " | Extra: " << inActualNotExpected.size() << std::endl;

            if (verbose && !inExpectedNotActual.empty())
            {
                std::cout << "\nMissing elements (first 5):" << std::endl;
                for (size_t i = 0; i < std::min<size_t>(5, inExpectedNotActual.size()); ++i)
                    std::cout << "  " << inExpectedNotActual[i].get<u64>(0) << std::endl;
            }
        }

        std::cout << "\n=== Performance ===" << std::endl;
        std::cout << "Initial comm: " << (double)initial_bytes / (1 << 20) << " MB" << std::endl;
        std::cout << "Update comm:  " << (double)update_bytes / (1 << 20) << " MB" << std::endl;
        std::cout << "Total comm:   " << (double)total_bytes / (1 << 20) << " MB" << std::endl;
        std::cout << "\nTotal Time: " << timer << std::endl;
        std::cout << "Sender Timer:\n"
                  << s << std::endl;
        std::cout << "Receiver Timer:\n"
                  << r << std::endl;

        if (initialUnionCorrect && finalUnionCorrect)
            std::cout << "\nALL TESTS PASSED!" << std::endl;
        else
            std::cout << "\nTESTS FAILED" << std::endl;
    }

} // namespace volePSI
