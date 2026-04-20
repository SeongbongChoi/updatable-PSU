#include "Psu_TBZ25_Tests.h"
#include "volePSI/Psu_TBZ25.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "coproto/Socket/AsioSocket.h"
#include "Common.h"

#include <set>
#include <algorithm>
#include <thread>

using namespace std;
using namespace volePSI;
using namespace oc;
using coproto::AsioSocket;
using coproto::LocalAsyncSocket;

namespace volePSI
{

    void Psu_TBZ25_perf_test(const oc::CLP &cmd)
    {
        u64 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 12));
        u64 nt = cmd.getOr("nt", 1);
        bool verbose = cmd.isSet("v");

        PRNG prng(oc::toBlock(123456));

        std::vector<block> sendSet(n);
        std::vector<block> recvSet(n);

        for (u64 i = 0; i < n; ++i)
            sendSet[i] = oc::toBlock(0, prng.get<u64>());

        for (u64 i = 0; i < n; ++i)
            recvSet[i] = oc::toBlock(0, prng.get<u64>());

        std::set<block> expectedUnion(sendSet.begin(), sendSet.end());
        expectedUnion.insert(recvSet.begin(), recvSet.end());

        Timer timer, s, r;
        auto sockets = AsioSocket::makePair();

        PsuTBZ25Sender sender;
        PsuTBZ25Receiver recver;

        block commonSeed = oc::toBlock(42, 42);
        sender.init(n, n, 40, commonSeed, nt);
        recver.init(n, n, 40, commonSeed, nt);

        recver.setTimer(r);
        sender.setTimer(s);

        timer.setTimePoint("start");
        s.setTimePoint("start");
        r.setTimePoint("start");

        std::thread t0([&]() { 
            coproto::sync_wait(sender.send(sendSet, sockets[0])); 
        });
        std::thread t1([&]() { 
            coproto::sync_wait(recver.recv(recvSet, sockets[1])); 
        });

        t0.join();
        t1.join();

        timer.setTimePoint("end");

        u64 actualUnionSize = recver.U.size();
        u64 total_bytes = sockets[0].bytesSent() + sockets[1].bytesSent();

        std::set<block> actualUnion(recver.U.begin(), recver.U.end());

        bool sizeCorrect = (actualUnionSize == expectedUnion.size());
        bool contentCorrect = (actualUnion == expectedUnion);

        std::vector<block> inExpectedNotActual;
        std::vector<block> inActualNotExpected;

        std::set_difference(
            expectedUnion.begin(), expectedUnion.end(),
            actualUnion.begin(), actualUnion.end(),
            std::back_inserter(inExpectedNotActual));

        std::set_difference(
            actualUnion.begin(), actualUnion.end(),
            expectedUnion.begin(), expectedUnion.end(),
            std::back_inserter(inActualNotExpected));

        std::cout << std::endl;
        std::cout << "TBZ25 Balanced PSU Performance Test Results" << std::endl;
        std::cout << "============================================" << std::endl;
        std::cout << "Set size (both): " << n << std::endl;

        std::cout << "\n=== Union Verification ===" << std::endl;
        std::cout << "Expected union size: " << expectedUnion.size() << std::endl;
        std::cout << "Actual union size:   " << actualUnionSize << std::endl;
        std::cout << "Size match: " << (sizeCorrect ? "✓" : "✗") << std::endl;
        std::cout << "Content match: " << (contentCorrect ? "✓" : "✗") << std::endl;

        if (!sizeCorrect || !contentCorrect) {
            std::cout << "\nSize difference: " << (int64_t)actualUnionSize - (int64_t)expectedUnion.size() << std::endl;
            std::cout << "Missing elements: " << inExpectedNotActual.size() << std::endl;
            std::cout << "Extra elements: " << inActualNotExpected.size() << std::endl;

            if (verbose) {
                if (!inExpectedNotActual.empty()) {
                    std::cout << "\nMissing elements (first 5):" << std::endl;
                    for (size_t i = 0; i < std::min<size_t>(5, inExpectedNotActual.size()); ++i)
                        std::cout << "  " << inExpectedNotActual[i].get<u64>(0) << std::endl;
                }

                if (!inActualNotExpected.empty()) {
                    std::cout << "\nExtra elements (first 5):" << std::endl;
                    for (size_t i = 0; i < std::min<size_t>(5, inActualNotExpected.size()); ++i)
                        std::cout << "  " << inActualNotExpected[i].get<u64>(0) << std::endl;
                }
            }
        }

        std::cout << "\n=== Communication ===" << std::endl;
        std::cout << "Total Comm = " << (double)total_bytes / (1 << 20) << " MB ("
                  << (double)total_bytes / (1 << 10) << " KB)" << std::endl;
        std::cout << "Sender Comm = " << (double)sockets[0].bytesSent() / (1 << 20) << " MB ("
                  << (double)sockets[0].bytesSent() / (1 << 10) << " KB)" << std::endl;
        std::cout << "Receiver Comm = " << (double)sockets[1].bytesSent() / (1 << 20) << " MB ("
                  << (double)sockets[1].bytesSent() / (1 << 10) << " KB)" << std::endl;

        std::cout << "\n=== Timing ===" << std::endl;
        std::cout << "Total Time: " << timer << std::endl;
        std::cout << "\nSender Timer:\n" << s << std::endl;
        std::cout << "Receiver Timer:\n" << r << std::endl;

        if (sizeCorrect && contentCorrect)
            std::cout << "\n✓✓✓ ALL TESTS PASSED! ✓✓✓" << std::endl;
        else
            std::cout << "\n✗✗✗ TESTS FAILED ✗✗✗" << std::endl;
    }
}