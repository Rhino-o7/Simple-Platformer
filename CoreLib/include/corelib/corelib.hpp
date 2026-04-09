#pragma once

namespace corelib {
    int run_client(int argc, char** argv);
    int run_network_client(int argc, char** argv, const char* uri, bool reset_save = false);
}

