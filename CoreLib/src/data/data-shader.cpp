#include <data/data-shader.hpp>
#include <config.hpp>

#include <fstream>
#include <cctype>

#ifdef __EMSCRIPTEN__
#include <algorithm>
#endif

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

#ifdef __EMSCRIPTEN__
    inline void replace_all(std::string& source, const std::string& from, const std::string& to) {
        if (from.empty()) {
            return;
        }

        size_t pos = 0;
        while ((pos = source.find(from, pos)) != std::string::npos) {
            source.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    inline void normalize_shader_for_web(std::string& source) {
        constexpr const char* gl330 = "#version 330 core";
        const auto pos = source.find(gl330);
        if (pos != std::string::npos) {
            source.replace(pos, std::char_traits<char>::length(gl330), "#version 300 es\nprecision highp float;\nprecision highp int;");
        }

        replace_all(source, "occlusion / NUM_SAMPLES", "occlusion / float(NUM_SAMPLES)");
    }
#endif
}

using namespace vpg;
using namespace vpg::data;

void* Shader::load(Asset* asset) {
    auto args = trim_whitespace(asset->get_args());
    std::string vs_path = Config::get_string("data.folder", "./data/") + args.substr(0, args.find(' '));
    std::string fs_path = Config::get_string("data.folder", "./data/") + args.substr(args.find(' ') + 1);
    vs_path = trim_whitespace(vs_path);
    fs_path = trim_whitespace(fs_path);

    // Load vertex shader
    std::string vs;
    std::ifstream vs_ifs(vs_path);
    if (!vs_ifs.is_open()) {
        std::cerr << "vpg::data::Shader::load() failed:\n"
                  << "Couldn't open file '" << vs_path << "'\n";
        return nullptr;
    }
    vs_ifs.seekg(0, std::ios::end);
    vs.reserve(vs_ifs.tellg());
    vs_ifs.seekg(0, std::ios::beg);
    vs.assign((std::istreambuf_iterator<char>(vs_ifs)),
        std::istreambuf_iterator<char>());
    vs_ifs.close();

#ifdef __EMSCRIPTEN__
    normalize_shader_for_web(vs);
#endif

    // Load fragment shader
    std::string fs;
    std::ifstream fs_ifs(fs_path);
    if (!fs_ifs.is_open()) {
        std::cerr << "vpg::data::Shader::load() failed:\n"
                  << "Couldn't open file '" << fs_path << "'\n";
        return nullptr;
    }
    fs_ifs.seekg(0, std::ios::end);
    fs.reserve(fs_ifs.tellg());
    fs_ifs.seekg(0, std::ios::beg);
    fs.assign((std::istreambuf_iterator<char>(fs_ifs)),
        std::istreambuf_iterator<char>());
    fs_ifs.close();

#ifdef __EMSCRIPTEN__
    normalize_shader_for_web(fs);
#endif

    auto shader = new Shader();

    if (!gl::Shader::create(shader->shader, vs.c_str(), fs.c_str())) {
        std::cerr << "vpg::data::Shader::load() failed:\n"
                  << "Couldn't create shader\n";
        delete shader;
        return nullptr;
    }

    return shader;
}

void Shader::unload(Asset* asset) {
    auto shader = (Shader*)asset->get_data();
    delete shader;
}




