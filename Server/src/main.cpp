#include <corelib/net/websocket.hpp>

#include <cstdint>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "Enter server port [9002]: ";

    std::string port_text;
    std::getline(std::cin, port_text);

    int port_input = 9002;
    if (!port_text.empty()) {
        try {
            size_t parsed_chars = 0;
            int parsed_port = std::stoi(port_text, &parsed_chars);

            if (parsed_chars != port_text.size() || parsed_port <= 0 || parsed_port > 65535) {
                std::cerr << "Invalid port\n";
                return 1;
            }

            port_input = parsed_port;
        }
        catch (...) {
            std::cerr << "Invalid port\n";
            return 1;
        }
    }

    corelib::net::GameServer server;
    if (!server.start(static_cast<uint16_t>(port_input))) {
        return 1;
    }

    std::cout << "Server running on port " << port_input << ". Press ENTER to stop.\n";
    std::cin.get();

    server.stop();
    return 0;
}
