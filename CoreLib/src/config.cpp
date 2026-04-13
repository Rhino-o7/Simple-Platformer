#include <config.hpp>

#include <fstream>
#include <iostream>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <vector>

using namespace vpg;

std::mutex Config::mutex;
std::map<std::string, std::string> Config::variables;

namespace {
	inline std::string trim_whitespace(std::string value) {
		size_t begin = 0;
		while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
			++begin;
		}

		size_t end = value.size();
		while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
			--end;
		}

		return value.substr(begin, end - begin);
	}
}

bool Config::load(int argc, char** argv) {
	std::string config_path = "./vpg.cfg"; // Defaut config file path

	// Parse variables from command-line arguments
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-c") == 0) {
			if (i + 1 >= argc) {
				std::cerr << "vpg::Config::load() failed:" << std::endl;
				std::cerr << "Command-line arguments parsing failed:" << std::endl;
				std::cerr << "Found '-c' but its argument is missing (unexpected end of string)" << std::endl;
				return false;
			}
			config_path = argv[++i];
		}
		else { // Parse key-value pair
			bool found = false;
			int j = 0;
			for (; argv[i][j] != '\0'; ++j) {
				if (argv[i][j] == '=') {
					found = true;
					++j;
					break;
				}
			}

			if (!found) {
				std::cerr << "vpg::Config::load() failed:" << std::endl;
				std::cerr << "Command-line arguments parsing failed:" << std::endl;
				std::cerr << "Expected 'key=value' pair, found \"" << argv[i] << "\"" << std::endl;
				return false;
			}

          auto key = std::string(argv[i]).substr(0, size_t(j - 1));
			auto value = std::string(&argv[i][j]);

			Config::variables.insert(std::make_pair(key, value));
		}
	}

 // Parse variables from config file
	std::ifstream fs(config_path);
	if (!fs.is_open()) {
		namespace fsys = std::filesystem;
		std::vector<fsys::path> candidates;

		try {
			auto cwd = fsys::current_path();
			for (auto p = cwd; !p.empty(); p = p.parent_path()) {
				candidates.emplace_back(p / "vpg.cfg");
				auto parent = p.parent_path();
				if (parent == p) {
					break;
				}
			}
		}
		catch (...) {
		}

		if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
			try {
				auto exe_dir = fsys::path(argv[0]).parent_path();
				for (auto p = exe_dir; !p.empty(); p = p.parent_path()) {
					candidates.emplace_back(p / "vpg.cfg");
					auto parent = p.parent_path();
					if (parent == p) {
						break;
					}
				}
			}
			catch (...) {
			}
		}

		for (const auto& candidate : candidates) {
			fs = std::ifstream(candidate);
			if (fs.is_open()) {
				config_path = candidate.string();
				break;
			}
		}
	}

	if (!fs.is_open()) {
		std::cerr << "vpg::Config::load() warning:" << std::endl;
		std::cerr << "Couldn't open configuration file on path \"" << config_path << "\"" << std::endl;
		std::cerr << "Using default configuration values" << std::endl;
		return true;
	}

	std::string line;
	while (std::getline(fs, line)) {
		// Remove comments
		int i = 0;
		for (; i < line.size() && line[i] != ';'; ++i);
		line = line.substr(0, i);
		line = trim_whitespace(line);
		if (line.empty())
			continue;

		// Search for assignment operator
		bool found = false;
		for (i = 0; i < line.size(); ++i) {
			if (line[i] == '=') {
				found = true;
				break;
			}
		}

		if (found) {
			auto key = line.substr(0, i);
			auto value = line.substr(size_t(i) + 1);
			key = trim_whitespace(key);
			value = trim_whitespace(value);

			if (key.empty()) {
				std::cerr << "vpg::Config::load() failed:" << std::endl;
				std::cerr << "Couldn't parse configuration file:" << std::endl;
				std::cerr << "Invalid 'key=value' pair on \"" << line << "\"" << std::endl;
				std::cerr << "The key string must not be empty" << std::endl;
				return false;
			}

			if (Config::variables.find(key) == Config::variables.end())
				Config::variables.insert(std::make_pair(key, value));
		}
		else {
			std::cerr << "vpg::Config::load() failed:" << std::endl;
			std::cerr << "Couldn't parse configuration file:" << std::endl;
			std::cerr << "Invalid 'key=value' pair on \"" << line << "\"" << std::endl;
			return false;
		}
	}

	return true;
}

bool vpg::Config::get_boolean(const std::string& key, bool def) {
	std::lock_guard guard(Config::mutex);

	if (Config::variables.find(key) == Config::variables.end()) {
		Config::variables[key] = std::to_string(def);
	}

	if (Config::variables[key] == "true") {
		return true;
	}
	else if (Config::variables[key] == "false") {
		return false;
	}
	else {
		Config::variables[key] = def ? "true" : "false";
		return def;
	}
}

int64_t Config::get_integer(const std::string& key, int64_t def) {
	std::lock_guard guard(Config::mutex);

	if (Config::variables.find(key) == Config::variables.end()) {
		Config::variables[key] = std::to_string(def);
	}

	try {
		return std::stoll(Config::variables[key]);
	}
	catch (...) {
		Config::variables[key] = std::to_string(def);
		return def;
	}
}

double Config::get_float(const std::string& key, double def) {
	std::lock_guard guard(Config::mutex);

	if (Config::variables.find(key) == Config::variables.end()) {
		Config::variables[key] = std::to_string(def);
	}

	try {
		return std::stod(Config::variables[key]);
	}
	catch (...) {
		Config::variables[key] = std::to_string(def);
		return def;
	}
}

std::string Config::get_string(const std::string& key, const std::string& def) {
	std::lock_guard guard(Config::mutex);

	if (Config::variables.find(key) == Config::variables.end()) {
		Config::variables[key] = def;
	}

	return Config::variables[key];
}

void vpg::Config::set_boolean(const std::string& key, bool value) {
	std::lock_guard guard(Config::mutex);
	Config::variables[key] = value ? "true" : "false";
}

void Config::set_integer(const std::string& key, int64_t value) {
	std::lock_guard guard(Config::mutex);
	Config::variables[key] = std::to_string(value);
}

void Config::set_float(const std::string& key, double value) {
	std::lock_guard guard(Config::mutex);
	Config::variables[key] = std::to_string(value);
}

void Config::set_string(const std::string& key, const std::string& value) {
	std::lock_guard guard(Config::mutex);
	Config::variables[key] = value;
}




