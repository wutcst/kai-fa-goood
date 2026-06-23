#include "GameServer.hpp"

#include "LevelCatalog.hpp"
#include "Protocol.hpp"
#include "RoomNetwork.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

namespace fireice {

std::string GameServer::generateRoomCode() {
    static const char kChars[] = "0123456789";
    std::string code;
    code.reserve(6);
    for (int i = 0; i < 6; ++i) {
        code.push_back(kChars[std::rand() % 10]);
    }
    return code;
}

bool GameServer::start() {
    if (socket_.bind(SERVER_PORT) != sf::Socket::Done) {
        std::cerr << "[Server] Failed to bind UDP port " << SERVER_PORT << std::endl;
        return false;
    }
    socket_.setBlocking(false);

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    running_ = true;
    lastTick_ = std::chrono::steady_clock::now();
    lastBroadcast_ = lastTick_;

    std::cout << "[Server] Multi-room server listening on port " << SERVER_PORT << std::endl;
    std::cout << "[Server] Levels loaded: " << static_cast<int>(LevelCatalog::instance().count()) << std::endl;
    return true;
}

void GameServer::run() {
    constexpr int kMaxCatchUpTicks = 8;
    const auto tickDuration =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(TICK_DT));

    while (running_) {
        processPackets();

        const auto now = std::chrono::steady_clock::now();
        float tickElapsed = std::chrono::duration<float>(now - lastTick_).count();
        int simulatedTicks = 0;
        while (tickElapsed >= TICK_DT && simulatedTicks < kMaxCatchUpTicks) {
            simulateAllRooms();
            tickElapsed -= TICK_DT;
            lastTick_ += tickDuration;
            ++simulatedTicks;
        }

        const float broadcastElapsed = std::chrono::duration<float>(now - lastBroadcast_).count();
        if (broadcastElapsed >= 1.0f / STATE_BROADCAST_HZ) {
            broadcastAllRooms();
            lastBroadcast_ = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void GameServer::stop() {
    running_ = false;
}

void GameServer::processPackets() {
    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t received = 0;
    sf::IpAddress sender;
    unsigned short port = 0;

    while (socket_.receive(buffer.data(), buffer.size(), received, sender, port) == sf::Socket::Done) {
        if (received < sizeof(PacketHeader)) {
            continue;
        }

        const auto* header = reinterpret_cast<const PacketHeader*>(buffer.data());

        switch (header->type) {
            case PacketType::ConnectRequest: {
                ConnectRequestPacket packet{};
                if (!unpackPacket(buffer.data(), received, packet)) {
                    break;
                }

                if (packet.roomCode[0] != '\0') {
                    const std::string targetCode(packet.roomCode);
                    auto it = rooms_.find(targetCode);
                    if (it != rooms_.end()) {
                        RoomNetwork::acceptClient(*it->second, socket_, sender, port, packet);
                    } else {
                        ConnectRejectPacket reject{};
                        std::snprintf(reject.reason, sizeof(reject.reason), "Room not found");
                        std::array<char, PACKET_BUFFER_SIZE> buf{};
                        std::size_t sz = 0;
                        packPacket(reject, buf, sz);
                        socket_.send(buf.data(), sz, sender, port);
                    }
                } else {
                    std::string newCode;
                    do {
                        newCode = generateRoomCode();
                    } while (rooms_.find(newCode) != rooms_.end());

                    auto serverRoom = std::make_unique<ServerRoom>();
                    serverRoom->simulation.code = newCode;
                    serverRoom->simulation.lastTick = std::chrono::steady_clock::now();
                    serverRoom->simulation.lastBroadcast = serverRoom->simulation.lastTick;
                    std::snprintf(serverRoom->simulation.world.roomCode, MAX_ROOM_CODE, "%s", newCode.c_str());
                    serverRoom->simulation.selectLevel(0);

                    ServerRoom* raw = serverRoom.get();
                    rooms_[newCode] = std::move(serverRoom);
                    RoomNetwork::acceptClient(*raw, socket_, sender, port, packet);

                    std::cout << "[Server] Created room " << newCode << std::endl;
                }
                break;
            }

            case PacketType::Input:
            case PacketType::Action:
            case PacketType::Disconnect: {
                ServerRoom* foundRoom = nullptr;
                uint8_t foundSlot = 0;
                for (auto& [code, room] : rooms_) {
                    auto slot = RoomNetwork::findSlotByEndpoint(*room, sender, port);
                    if (slot.has_value()) {
                        foundRoom = room.get();
                        foundSlot = slot.value();
                        break;
                    }
                }
                if (!foundRoom) {
                    break;
                }

                Room& sim = foundRoom->simulation;
                if (header->type == PacketType::Input) {
                    InputPacket pkt{};
                    if (unpackPacket(buffer.data(), received, pkt) && pkt.slot == foundSlot) {
                        if (sim.world.phase == GamePhase::Playing) {
                            sim.clients[foundSlot].pendingInput = static_cast<InputFlags>(pkt.flags);
                        }
                    }
                } else if (header->type == PacketType::Action) {
                    ActionPacket pkt{};
                    if (unpackPacket(buffer.data(), received, pkt)) {
                        if (pkt.slot != foundSlot) {
                            std::cerr << "[Server] Action slot mismatch: packet=" << static_cast<int>(pkt.slot)
                                      << " endpoint=" << static_cast<int>(foundSlot) << std::endl;
                        }
                        sim.handleAction(foundSlot, pkt.action, pkt.value);
                        RoomNetwork::broadcastState(*foundRoom, socket_);
                    }
                } else if (header->type == PacketType::Disconnect) {
                    DisconnectPacket pkt{};
                    if (unpackPacket(buffer.data(), received, pkt) && pkt.slot == foundSlot) {
                        RoomNetwork::disconnectClient(*foundRoom, foundSlot, socket_);
                        bool empty = true;
                        for (const auto& c : sim.clients) {
                            if (c.connected) {
                                empty = false;
                                break;
                            }
                        }
                        if (empty) {
                            std::cout << "[Server] Removing empty room " << sim.code << std::endl;
                            rooms_.erase(sim.code);
                        }
                    }
                }
                break;
            }

            case PacketType::Discovery: {
                DiscoveryPacket disc{};
                if (!unpackPacket(buffer.data(), received, disc) || disc.isResponse != 0) {
                    break;
                }

                const std::string requested(disc.roomCode);
                for (auto& [code, room] : rooms_) {
                    if (requested.empty() || requested == code) {
                        const Room& sim = room->simulation;
                        DiscoveryPacket resp{};
                        resp.isResponse = 1;
                        std::snprintf(resp.roomCode, MAX_ROOM_CODE, "%s", code.c_str());
                        resp.playerCount = sim.world.connectedCount;
                        resp.maxPlayers = MAX_PLAYERS;
                        const LevelCatalog& cat = LevelCatalog::instance();
                        if (sim.selectedLevelIndex < cat.count()) {
                            std::snprintf(resp.levelName, MAX_LEVEL_NAME, "%s", cat.at(sim.selectedLevelIndex).title);
                        }
                        std::array<char, PACKET_BUFFER_SIZE> buf{};
                        std::size_t sz = 0;
                        packPacket(resp, buf, sz);
                        socket_.send(buf.data(), sz, sender, port);
                        if (!requested.empty()) {
                            break;
                        }
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void GameServer::simulateAllRooms() {
    for (auto& [code, room] : rooms_) {
        room->simulation.simulateTick();
    }
}

void GameServer::broadcastAllRooms() {
    for (auto& [code, room] : rooms_) {
        RoomNetwork::broadcastState(*room, socket_);
    }
}

}  // namespace fireice
