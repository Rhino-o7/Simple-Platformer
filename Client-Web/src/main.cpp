#include <corelib/corelib.hpp>

#include <string>

int main(int argc, char** argv) {
    std::string uri = "ws://127.0.0.1:9002";
    bool reset_save = false;

    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        uri = argv[1];
    }

    if (argc > 2 && argv[2] != nullptr) {
        reset_save = std::string(argv[2]) == "reset";
    }

    return corelib::run_network_client(argc, argv, uri.c_str(), reset_save);
}
