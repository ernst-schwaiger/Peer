#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include "CommonTypes.h"
#include "TestEnvironment.h"

using namespace std;

namespace rgc
{
    static payload_t mkPayload(MessageId msgId, string s)
    {
        payload_t ret;
        ret.reserve(s.length() + 4);
        ret.push_back(msgId.getPeerId() >> 8 );
        ret.push_back(msgId.getPeerId() & 0xff );
        ret.push_back(msgId.getSeqNr() >> 8 );
        ret.push_back(msgId.getSeqNr() & 0xff );

        ret.insert(end(ret), begin(s), end(s));

        return ret;
    }

    TEST_CASE( "DummyTest", "MiddleWare" )
    {
        TestTxSocket txSocket({ 1, 42, "192.168.1.1"});
        vector<ITxSocket*> txSockets;
        txSockets.push_back(&txSocket);
        TestRxSocket rxSocket;

        rxSocket.m_receivedPayloads.push_back(mkPayload({ 1, 1} , "test"));
        TestApp testApp(&rxSocket, txSockets, 10);
        testApp.run();

        REQUIRE(txSocket.m_sentPayloads.size() == 1);

    }
}