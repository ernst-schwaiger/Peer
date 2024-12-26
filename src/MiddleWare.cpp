#include <iostream>
#include <string>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "MiddleWare.h"

using namespace std;
using namespace rgc;

static constexpr std::chrono::duration<int64_t, std::milli> ACK_TIMEOUT = std::chrono::milliseconds(1000);

// void MiddleWare::doSend()
// {
//     cout << "Sending Udp Packet\n";

//     int sockfd;
//     struct sockaddr_in server_addr;
//     char const *message = "Hello, UDP!";

//     // Create a socket
//     if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
//         perror("socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     // Define the server address
//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(12345);
//     server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

//     // Send the message
//     if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
//         perror("sendto failed");
//         close(sockfd);
//         exit(EXIT_FAILURE);
//     }

//     cout << "Message sent.\n";

//     // Close the socket
//     close(sockfd);
// }


void MiddleWare::rxTxLoop()
{
    while (!m_stop)
    {
        listenRxSocket();
        checkPendingTxMessages();
        checkPendingCommands();
        usleep(100000); // Sleep for 100 milliseconds
    }
}

void MiddleWare::listenRxSocket()
{
    rx_buffer_t buf;
    struct sockaddr_in remoteSockAddr;
    
    // Polling for incoming data...
    rgc::TransmitStatus status = m_pRxSocket->receive(buf, remoteSockAddr);

    if (status.status != 0)
    {
        // FIXME error handling
    }
    else
    {
        if (status.transmitBytes > 0)
        {
            payload_t payload(&buf[0], &buf[status.transmitBytes]);
            processRxMessage(payload, remoteSockAddr);
        }
    }
}

void MiddleWare::checkPendingTxMessages()
{
    auto now = std::chrono::system_clock::now();

    for (auto &txMsgState : m_txMessageStates)
    {
        vector<TxState> &txStates = txMsgState.getTxStates();
        for (auto &txState : txStates)
        {
            if (!txState.isAcknowledged() && txState.isTimeoutElapsed(now))
            {
                processTxMessage(txState, txMsgState.getPayload(), now);
            }
        }

        bool allAck = accumulate(begin(txStates), end(txStates), true, 
            [](bool acc, auto &e)
            {
                return (acc && e.isAcknowledged());
            });

        if (allAck)
        {
            m_pApp->deliverMessage(txMsgState.getMsgId(), txMsgState.getPayload());
            // FIXME: Deliver message
            // FIXME: Dispose of this element
        }
    }
}

void MiddleWare::checkPendingCommands()
{
    // FIXME: Implement this
}

void MiddleWare::processTxMessage(TxState &txState, vector<char> const &msg, std::chrono::system_clock::time_point const &now)
{
    uint8_t remainingTxAttempts = txState.getRemainingTxAttempts();
    if (remainingTxAttempts == 0)
    {
        // we did not get an ACK after the third tx attempt. we handle this case
        // as if it got an ACK
        txState.setAcknowledged();
    }
    else
    {
        txState.getSocket()->send(msg);
        std::chrono::system_clock::time_point timeout = now + ACK_TIMEOUT;
        txState.setTimeout(timeout);
        txState.setRemainingTxAttempts(remainingTxAttempts - 1);
    }
}

void MiddleWare::processRxMessage(rgc::payload_t &payload, struct sockaddr_in const &remoteSockAddr)
{
    // FIXME: Add CRC field
    if (payload.size() < sizeof(peerId_t) + sizeof(seqNr_t))
    {
        // truncated frame, ignore
    }
    else
    {
        bool isAckFrame = (payload.size() == sizeof(peerId_t) + sizeof(seqNr_t));
        peerId_t peerId = (payload[0] << 8) + payload[1];
        seqNr_t seqNr = (payload[2] << 8) + payload[3];

        // FIXME: Check for valid peer id
        // FIXME: Check that message with msgId has not been delivered yet
        // FIXME: Error injection

        MessageId msgId = MessageId(peerId, seqNr);
        TxMessageState *txMsgState = findTxMsgState(msgId);

        if (txMsgState == nullptr)
        {
            if (isAckFrame)
            {
                // No such message found in the state, set up anew
                m_txMessageStates.emplace_back(TxMessageState(msgId, m_txSockets, payload));
            }
        }
        else
        {
            // Message found, check content
            if (isAckFrame)
            {
                TxState *txState = txMsgState->findTxState(remoteSockAddr);
                if (txState != nullptr)
                {
                    txState->setAcknowledged();
                }
            }
            else
            {
                // We have received that message already, ignore it here
            }
        }
    }
}
