#include <corelib/corelib.hpp>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    bool reset_save = false;
    std::cout << "Save menu:\n"
              << "  1) Resume saved progress\n"
              << "  2) Start new progress\n"
              << "Select [1]: ";
    std::string save_option;
    std::getline(std::cin, save_option);
    if (save_option == "2") {
        reset_save = true;
    }

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
    int exit_code = corelib::run_network_client(argc, argv, uri.c_str(), reset_save);
    if (exit_code != 0) {
        std::cout << "Client exited with code " << exit_code << "\nPress ENTER to close...";
        std::string sink;
        std::getline(std::cin, sink);
    }
    return exit_code;
}
