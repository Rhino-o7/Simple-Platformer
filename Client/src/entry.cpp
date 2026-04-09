#include "../../CoreLib/include/corelib/corelib.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::filesystem::path executable_dir = std::filesystem::current_path();
    if (argc > 0 && argv != nullptr && argv[0] != nullptr && argv[0][0] != '\0') {
        executable_dir = std::filesystem::absolute(argv[0]).parent_path();
    }

    std::cout << "Save menu:\n"
              << "  1) Resume saved progress\n"
              << "  2) Start new progress\n"
              << "Select [1]: ";
    std::string save_option;
    std::getline(std::cin, save_option);
    if (save_option == "2") {
        std::error_code ec;
        std::filesystem::remove(executable_dir / "client_save.txt", ec);
    }

    std::cout << "Tip: Press F5 in-game to save progress.\n";
    return corelib::run_client(argc, argv);
}
