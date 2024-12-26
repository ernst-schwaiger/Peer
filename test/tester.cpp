#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

class UdpTxSocket final
{
public:
    UdpTxSocket(string const &remoteIp, uint16_t remotePort)
    {
        std::memset(&m_remoteSockAddr, 0, sizeof(m_remoteSockAddr));
        // FIXME: Only IP V4?
        m_remoteSockAddr.sin_family = AF_INET;
        m_remoteSockAddr.sin_addr.s_addr = inet_addr(remoteIp.c_str());
        m_remoteSockAddr.sin_port = htons(remotePort);
        // FIXME: Throw if we don't get a valid descriptor
        m_socketDesc = socket(AF_INET, SOCK_DGRAM, 0);
    }

    ~UdpTxSocket()
    {
        cout << "Closing tx socket.\n";
        close(m_socketDesc);
    }

    ssize_t send(string const &msg)
    {
        ssize_t sentBytes = sendto(
            m_socketDesc, 
            msg.c_str(), 
            msg.length(), 
            0, 
            reinterpret_cast<struct sockaddr*>(&m_remoteSockAddr), 
            sizeof(m_remoteSockAddr));
        return sentBytes;
    }

private:
    int m_socketDesc;
    struct sockaddr_in m_remoteSockAddr;
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <text>\n";
        cerr << "Sends <text> to 127.0.0.1:4242\n";
        return 1;
    }

    cout << "Sending a package...\n";
    UdpTxSocket txSock("127.0.0.1", 4242);
    txSock.send(argv[1]);
    return 0;
}