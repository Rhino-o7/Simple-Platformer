#include <corelib/corelib.hpp>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string uri;
    if (argc > 1 && argv[1] != nullptr) {
        uri = argv[1];
    }
    else {
        std::string host;
        std::string port;

        std::cout << "Server host [127.0.0.1]: ";
        std::getline(std::cin, host);
        if (host.empty()) {
            host = "127.0.0.1";
        }

        std::cout << "Server port [9002]: ";
        std::getline(std::cin, port);
        if (port.empty()) {
            port = "9002";
        }

        uri = "ws://" + host + ":" + port;
    }

    std::cout << "Connecting to " << uri << "\n";
    int exit_code = corelib::run_network_client(argc, argv, uri.c_str());
    if (exit_code != 0) {
        std::cout << "Client exited with code " << exit_code << "\nPress ENTER to close...";
        std::string sink;
        std::getline(std::cin, sink);
    }
    return exit_code;
}
