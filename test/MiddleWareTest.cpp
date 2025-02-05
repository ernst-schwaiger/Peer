#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include "CommonTypes.h"
#include "TestEnvironment.h"

using namespace std;

namespace rgc
{
    static const peer_t PEER_1 = { 1, 42, inet_addr("192.168.1.1") };
    static const peer_t PEER_1_incrrectPort = { 1, 41, inet_addr("192.168.1.1") };
    static const peer_t PEER_1_incrrectIP = { 1, 42, inet_addr("192.168.1.101") };

    static const peer_t PEER_2 = { 2, 43, inet_addr("192.168.1.2") };
    static const peer_t PEER_3 = { 3, 44, inet_addr("192.168.1.3") };

    static sender_payload_t mkRxPayload(peer_t const &sender, seqNr_t seqNr, string s = "")
    {
        sender_payload_t ret;
        ret.payload.reserve(s.length() + 6);
        ret.payload.push_back(sender.peerId >> 8 );
        ret.payload.push_back(sender.peerId & 0xff );
        ret.payload.push_back(seqNr >> 8 );
        ret.payload.push_back(seqNr & 0xff );
        ret.payload.insert(end(ret.payload), begin(s), end(s));
        checksum_t checksum = MiddleWare::rfc1071Checksum(ret.payload.data(), ret.payload.size());
        ret.payload.push_back(checksum >> 8);
        ret.payload.push_back(checksum & 0xff);

        ret.peer = sender;
        return ret;
    }

    static sender_payload_t mkRxResendPayload(peer_t const &sender, peer_t const &originator, seqNr_t seqNr, string s = "")
    {
        sender_payload_t ret;
        ret.payload.reserve(s.length() + 6);
        ret.payload.push_back(originator.peerId >> 8 );
        ret.payload.push_back(originator.peerId & 0xff );
        ret.payload.push_back(seqNr >> 8 );
        ret.payload.push_back(seqNr & 0xff );
        ret.payload.insert(end(ret.payload), begin(s), end(s));
        checksum_t checksum = MiddleWare::rfc1071Checksum(ret.payload.data(), ret.payload.size());
        ret.payload.push_back(checksum >> 8);
        ret.payload.push_back(checksum & 0xff);

        ret.peer = sender;
        return ret;
    }



    class Peers
    {
    public:
        Peers(std::vector<peer_t> peers) : 
            txSocks(mkTxSocks(peers)),
            rxSocket(),
            txISocks(mkITxSocks(txSocks)),
            app(&rxSocket, txISocks)
        {}
        
        vector<TestTxSocket> txSocks;
        TestRxSocket rxSocket;
        std::vector<ITxSocket*> txISocks;
        TestApp app;
    private:

        static std::vector<TestTxSocket> mkTxSocks(std::vector<peer_t> peers)
        {
            std::vector<TestTxSocket> ret;
            for (peer_t peer : peers)
            {
                ret.push_back(TestTxSocket(peer));
            }

            return ret;            
        }

        static std::vector<ITxSocket*> mkITxSocks(std::vector<TestTxSocket> &txSocks)
        {
            std::vector<ITxSocket*> ret;
            for (auto &txSock : txSocks)
            {
                ret.push_back(&txSock);
            }
            return ret;
        }
    };

    TEST_CASE( "Node does not send anything w/o prior receiving something", "MiddleWare" )
    {
        Peers p({PEER_1});
        
        // Simulate ten seconds run without receiving anything, our node shall send nothing
        p.app.numLoops(100).run(); 
        REQUIRE(p.txSocks[0].m_sentPayloads.empty());
        REQUIRE(p.app.deliveredMsgs.empty());
    }

    TEST_CASE( "Node shall ignore a truncated message", "MiddleWare" )
    {
        Peers p({PEER_1});
        
        sender_payload_t truncated;
        truncated.payload.push_back(0x00);
        truncated.payload.push_back(0x01);
        truncated.payload.push_back(0x00);
        truncated.payload.push_back(0x01);
        truncated.payload.push_back(0x02);
        truncated.peer = PEER_1;

        // receiving this truncated message shall be ignored by the middleware
        p.rxSocket.m_receivedPayloads.push_back(truncated);
        p.app.numLoops(100).run(); 
        REQUIRE(p.txSocks[0].m_sentPayloads.empty());
        REQUIRE(p.app.deliveredMsgs.empty());
    }

    TEST_CASE( "Node shall ignore a message with incorrect CRC", "MiddleWare" )
    {
        Peers p({PEER_1});
        
        sender_payload_t incorrectCRC;
        incorrectCRC.payload.push_back(0x00);
        incorrectCRC.payload.push_back(0x01);
        incorrectCRC.payload.push_back(0x00);
        incorrectCRC.payload.push_back(0x01);
        // incorrect CRC
        incorrectCRC.payload.push_back(0xca);
        incorrectCRC.payload.push_back(0xfe);

        incorrectCRC.peer = PEER_1;

        // receiving this message with incorrect CRC shall be ignored by the middleware
        p.rxSocket.m_receivedPayloads.push_back(incorrectCRC);
        p.app.numLoops(100).run(); 
        REQUIRE(p.txSocks[0].m_sentPayloads.empty());
        REQUIRE(p.app.deliveredMsgs.empty());
    }

    TEST_CASE( "Node shall ignore a message with an uknown Peer Id", "MiddleWare" )
    {
        Peers p({PEER_1});
        
        sender_payload_t unknownPeer;
        // Incorrect Peer Id in the header
        unknownPeer.payload.push_back(0xaf);
        unknownPeer.payload.push_back(0xfe);
        unknownPeer.payload.push_back(0x00);
        unknownPeer.payload.push_back(0x01);
        unknownPeer.payload.push_back(0xaf);
        unknownPeer.payload.push_back(0xfe);

        unknownPeer.peer = PEER_1;

        // receiving this message with incorrect CRC shall be ignored by the middleware
        p.rxSocket.m_receivedPayloads.push_back(unknownPeer);
        p.app.numLoops(100).run(); 
        REQUIRE(p.txSocks[0].m_sentPayloads.empty());
        REQUIRE(p.app.deliveredMsgs.empty());
    }

    TEST_CASE( "Node shall beable to process binary data in a message", "MiddleWare" )
    {
        Peers p({PEER_1});
        
        sender_payload_t binaryData;
        // Message Id
        binaryData.payload.push_back(0x00);
        binaryData.payload.push_back(0x01);
        binaryData.payload.push_back(0x00);
        binaryData.payload.push_back(0x01);

        // Binary Data
        binaryData.payload.push_back(0x12);
        binaryData.payload.push_back(0x34);

        // CRC
        binaryData.payload.push_back(0xed);
        binaryData.payload.push_back(0xcb);

        binaryData.peer = PEER_1;

        // receiving this message with binary datas
        p.rxSocket.m_receivedPayloads.push_back(binaryData);
        p.app.numLoops(1).run();
        // ACK and immediate retransmission
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2);
    }    


    TEST_CASE( "One Peer sending no ACK", "MiddleWare" )
    {
        // One peer
        Peers p({PEER_1});

        // Simulate reception from peer
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 1, "test"));
        p.app.numLoops(1).run();
        // Our node shall have sent the ack and the retransmit
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2);
        p.app.numLoops(9).run();
        // Our node shall wait for an ACK to arrive
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2);
        p.app.numLoops(1).run();
        // No ACK arrived, our node repeats transmission
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 3);
        p.app.numLoops(9).run();
        // Our node shall wait for an ACK to arrive, no message delivered to app
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 3);
        p.app.numLoops(1).run();
        // No ACK arrived, our node repeats transmission
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 4);
        p.app.numLoops(9).run();
        // Our node shall wait for an ACK to arrive, no message delivered to app
        REQUIRE(p.app.deliveredMsgs.empty());
        p.app.numLoops(1).run();
        REQUIRE(p.app.deliveredMsgs.size() == 1);
        // Nothing more shall happen in our node
        p.app.numLoops(100).run(); 
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 4); 
        REQUIRE(p.app.deliveredMsgs.size() == 1);
    }

    TEST_CASE( "Three Peers sending ACKs", "MiddleWare" )
    {
        // One peer
        Peers p({PEER_1, PEER_2, PEER_3});
        
        // Simulate reception from peer 1
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 0, "test1"));
        p.app.numLoops(1).run();
        
        // Our node shall have sent the messages back immediately
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2); // One Ack and one retransmit sent to peer 1
        REQUIRE(p.txSocks[1].m_sentPayloads.size() == 0); // Resend to Peer 2 immediately
        REQUIRE(p.txSocks[2].m_sentPayloads.size() == 0); // Resend is deferred by one second

        // We receive the ack from Peer 1...
        p.rxSocket.m_receivedPayloads.push_back(mkRxResendPayload(PEER_1, PEER_1, 0));
        // ... and wait until... 
        p.app.numLoops(9).run();
        REQUIRE(p.txSocks[1].m_sentPayloads.size() == 0);
        // ... our Peer resends to Peer 2
        p.app.numLoops(1).run();
        REQUIRE(p.txSocks[1].m_sentPayloads.size() == 1);
        // We then receive the ack from Peer 2...
        p.rxSocket.m_receivedPayloads.push_back(mkRxResendPayload(PEER_2, PEER_1, 0));

        // ... and wait until... 
        p.app.numLoops(9).run();
        REQUIRE(p.txSocks[2].m_sentPayloads.size() == 0);
        // ... our Peer resends to Peer 3
        p.app.numLoops(1).run();
        REQUIRE(p.txSocks[2].m_sentPayloads.size() == 1);
        // We then receive the ack from Peer 3...
        p.rxSocket.m_receivedPayloads.push_back(mkRxResendPayload(PEER_3, PEER_1, 0));
        REQUIRE(p.app.deliveredMsgs.size() == 0);
        p.app.numLoops(1).run();
        // ... which will trigger the delivery of the message to our app layer
        REQUIRE(p.app.deliveredMsgs.size() == 1);
    }


    TEST_CASE( "One Peer sending ACKs immediately", "MiddleWare" )
    {
        // One peer
        Peers p({PEER_1});
        // Simulate reception from peer
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 1,"test"));
        p.app.numLoops(1).run();
        // Must have been answered with ack and must be resent immediately
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2);
        // Simulate ack reception
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 1));
        p.app.numLoops(1).run();
        // Message must have been delivered  
        REQUIRE(p.app.deliveredMsgs.size() == 1);
        p.app.numLoops(100).run();
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2);
        REQUIRE(p.app.deliveredMsgs.size() == 1);
    }

    TEST_CASE( "Unknown Peers message must be discarded", "MiddleWare" )
    {
        // One peer
        Peers p({PEER_1});
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_2, 1, "test"));
        p.app.numLoops(100).run();
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 0);
    }

    TEST_CASE( "Messages from unknown IP/Udp Port Number combinations must be discarded", "MiddleWare" )
    {
        // One peer
        Peers p({PEER_1});

        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1_incrrectPort, 1, "test"));
        p.app.numLoops(100).run();
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 0);

        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1_incrrectIP, 1, "test"));
        p.app.numLoops(100).run();
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 0);
    }

    TEST_CASE( "A SeqNr of the same Peer must not be delivered twice", "MiddleWare" )
    {
        // One peer
        Peers p({PEER_1});
        // Simulate reception from peer
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 1,"test"));
        p.app.numLoops(1).run();
        // Must have been Acked and resent immediately
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 2);
        // Simulate ack reception
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 1));
        p.app.numLoops(1).run();
        // Message must have been delivered  
        REQUIRE(p.app.deliveredMsgs.size() == 1);

        // Same seq number received again, now we send back an ACK, but we discard it
        p.rxSocket.m_receivedPayloads.push_back(mkRxPayload(PEER_1, 1,"test"));
        p.app.numLoops(1).run();
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 3);
        p.app.numLoops(100).run();
        REQUIRE(p.txSocks[0].m_sentPayloads.size() == 3);
        REQUIRE(p.app.deliveredMsgs.size() == 1);
    }
}