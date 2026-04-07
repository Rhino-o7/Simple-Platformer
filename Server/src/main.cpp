#include <corelib/net/websocket.hpp>

#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    std::cout << "Enter server port: ";

    int port_input = 0;
    std::cin >> port_input;
    if (!std::cin.good() || port_input <= 0 || port_input > 65535) {
        std::cerr << "Invalid port\n";
        return 1;
    }

    corelib::net::GameServer server;
    if (!server.start(static_cast<uint16_t>(port_input))) {
        return 1;
    }

    std::cout << "Server running on port " << port_input << ". Press ENTER to stop.\n";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    server.stop();
    return 0;
}
