#include "GameClient.hpp"

#include <iostream>
#include <string>

namespace {

fireice::PlayerRole parseRole(const std::string& text) {
    if (text == "fire" || text == "f" || text == "1") {
        return fireice::PlayerRole::Fire;
    }
    if (text == "water" || text == "w" || text == "2") {
        return fireice::PlayerRole::Water;
    }
    if (text == "poison" || text == "p" || text == "3") {
        return fireice::PlayerRole::Poison;
    }
    return fireice::PlayerRole::Fire;
}

void printUsage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << " [host] [role]\n\n"
              << "Examples:\n"
              << "  " << program << "                  # public server, fire boy\n"
              << "  " << program << " 8.141.101.126 water  # custom server, water girl\n\n"
              << "Title screen:\n"
              << "  Up/Down         Select menu item\n"
              << "  Enter           Confirm\n"
              << "  Mouse           Click menu item\n\n"
              << "Lobby:\n"
              << "  Up/Down or 1-8  Select level\n"
              << "  ENTER           Ready / cancel ready\n\n"
              << "In-game:\n"
              << "  Fire  -> WASD    Water -> Arrows\n"
              << "  ESC   -> Return to lobby (anytime)\n\n"
              << "Result:\n"
              << "  R  Replay    N  Next level    ESC  Lobby\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    std::string host = fireice::DEFAULT_SERVER_HOST;
    fireice::PlayerRole role = fireice::PlayerRole::Fire;
    bool autoConnect = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--auto" || arg == "--join") {
            autoConnect = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (host == "127.0.0.1" && arg.find('.') != std::string::npos) {
            host = arg;
        } else if (arg == "fire" || arg == "water" || arg == "poison" || arg == "f" || arg == "w" || arg == "p" ||
                   arg == "1" || arg == "2" || arg == "3") {
            role = parseRole(arg);
        }
    }

    fireice::GameClient client;
    if (!client.initialize(host, role, autoConnect)) {
        return 1;
    }

    client.run();
    return 0;
}
