#pragma once

#include "Protocol.hpp"

#include <SFML/Network.hpp>

#include <functional>
#include <string>

namespace fireice {

// 客户端 UDP 通信层：connect / input / action / state 收发
class ClientNetwork {
public:
    using ConnectAcceptHandler = std::function<void(const ConnectAcceptPacket&)>;
    using ConnectRejectHandler = std::function<void(const ConnectRejectPacket&)>;
    using StateHandler = std::function<void(const StatePacket&)>;

    bool bindLocal();
    unsigned short localPort() const { return localPort_; }

    void setServerHost(const std::string& host);
    const sf::IpAddress& serverAddress() const { return serverAddress_; }

    bool sendConnectRequest(const ConnectRequestPacket& request);
    bool sendInput(uint8_t slot, InputFlags input, uint32_t tick);
    bool sendAction(uint8_t slot, PlayerAction action, uint8_t value = 0);
    bool sendDisconnect(uint8_t slot);

    void poll(const ConnectAcceptHandler& onAccept, const ConnectRejectHandler& onReject, const StateHandler& onState);

private:
    sf::UdpSocket socket_;
    sf::IpAddress serverAddress_;
    unsigned short localPort_ = 0;
};

}  // namespace fireice
