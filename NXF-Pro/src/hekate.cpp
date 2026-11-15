/*
 * NXF Pro - hekate boot configuration.
 *
 * MIT licensed, see LICENSE.
 */
#include "nxf/hekate.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace nxf {
namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool readWholeFile(const std::string& path, std::string& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    char buffer[4096];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        out.append(buffer, read);
    }
    std::fclose(file);
    return true;
}

} // namespace

HekateConfig readHekateConfig(const std::string& path) {
    HekateConfig config;

    std::string text;
    if (!readWholeFile(path, text)) {
        return config;
    }
    config.exists = true;

    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = trim(text.substr(start, end - start));
        if (!line.empty()) {
            if (line.front() == '[' && line.back() == ']') {
                const std::string name = trim(line.substr(1, line.size() - 2));
                if (name != "config") {
                    config.entries.push_back(name);
                }
            } else if (line.compare(0, 9, "autoboot=") == 0) {
                config.autoboot = std::atoi(line.c_str() + 9);
            }
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1;
    }

    return config;
}

bool writeHekateAutoboot(const std::string& path, int autoboot, std::string& error) {
    std::string text;
    readWholeFile(path, text);

    std::string result;
    bool written = false;
    bool inConfig = false;
    bool hasConfig = false;

    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        const bool last = end == std::string::npos;
        if (last) {
            end = text.size();
        }
        const std::string line = text.substr(start, end - start);
        const std::string clean = trim(line);

        if (!clean.empty() && clean.front() == '[' && clean.back() == ']') {
            const std::string name = trim(clean.substr(1, clean.size() - 2));
            if (inConfig && !written) {
                result += "autoboot=" + std::to_string(autoboot) + "\n";
                written = true;
            }
            inConfig = name == "config";
            hasConfig = hasConfig || inConfig;
        }

        if (inConfig && clean.compare(0, 9, "autoboot=") == 0) {
            if (!written) {
                result += "autoboot=" + std::to_string(autoboot) + "\n";
                written = true;
            }
        } else {
            result += line;
            result += "\n";
        }

        if (last) {
            break;
        }
        start = end + 1;
    }

    if (inConfig && !written) {
        result += "autoboot=" + std::to_string(autoboot) + "\n";
        written = true;
    }
    if (!hasConfig) {
        result = "[config]\nautoboot=" + std::to_string(autoboot) + "\nbootwait=3\n" + result;
    }

    mkdir("sdmc:/bootloader", 0777);

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        error = "Écriture impossible";
        return false;
    }
    const std::size_t size = result.size();
    const bool ok = std::fwrite(result.data(), 1, size, file) == size;
    std::fclose(file);
    if (!ok) {
        error = "Écriture incomplète";
    }
    return ok;
}

} // namespace nxf
