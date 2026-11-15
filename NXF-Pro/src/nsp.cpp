/*
 * NXF Pro - NSP (PFS0) container inspection.
 *
 * MIT licensed, see LICENSE.
 */
#include "nxf/nsp.hpp"

#include <cstdio>
#include <cstring>

namespace nxf {
namespace {

constexpr std::uint32_t kPfs0Magic = 0x30534650; // "PFS0"
constexpr std::uint32_t kMaxEntries = 4096;

std::uint32_t readU32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::uint64_t readU64(const std::uint8_t* data) {
    return static_cast<std::uint64_t>(readU32(data)) |
           (static_cast<std::uint64_t>(readU32(data + 4)) << 32);
}

bool endsWith(const std::string& text, const char* suffix) {
    const std::size_t length = std::strlen(suffix);
    return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
}

} // namespace

NspInfo parseNsp(const std::string& path) {
    NspInfo info;

    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        info.error = "Fichier illisible";
        return info;
    }

    std::uint8_t header[16] = {};
    if (std::fread(header, 1, sizeof(header), file) != sizeof(header)) {
        std::fclose(file);
        info.error = "Fichier trop court";
        return info;
    }
    if (readU32(header) != kPfs0Magic) {
        std::fclose(file);
        info.error = "Ce n'est pas un NSP (PFS0 attendu)";
        return info;
    }

    const std::uint32_t count = readU32(header + 4);
    const std::uint32_t stringTable = readU32(header + 8);
    if (count == 0 || count > kMaxEntries) {
        std::fclose(file);
        info.error = "Table PFS0 invalide";
        return info;
    }

    std::vector<std::uint8_t> table(static_cast<std::size_t>(count) * 24u + stringTable);
    std::fseek(file, 16, SEEK_SET);
    if (std::fread(table.data(), 1, table.size(), file) != table.size()) {
        std::fclose(file);
        info.error = "Table PFS0 tronquée";
        return info;
    }

    const std::uint8_t* names = table.data() + static_cast<std::size_t>(count) * 24u;
    const std::uint64_t dataStart = 16ull + table.size();

    info.version = readU32(header + 12);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = table.data() + static_cast<std::size_t>(i) * 24u;
        const std::uint64_t offset = readU64(entry);
        const std::uint64_t size = readU64(entry + 8);
        const std::uint32_t nameOffset = readU32(entry + 16);

        if (nameOffset >= stringTable) {
            continue;
        }
        const char* name = reinterpret_cast<const char*>(names + nameOffset);
        const std::size_t room = stringTable - nameOffset;
        std::size_t length = 0;
        while (length < room && name[length] != '\0') {
            ++length;
        }
        if (length == 0 || length >= room) {
            continue;
        }

        NspContent content;
        content.name.assign(name, length);
        content.offset = dataStart + offset;
        content.size = size;
        info.contents.push_back(content);
        info.total += size;

        if (endsWith(content.name, ".nca")) {
            ++info.ncaCount;
            if (content.name.find("cnmt") != std::string::npos) {
                info.hasCnmt = true;
            }
        } else if (endsWith(content.name, ".tik")) {
            info.hasTicket = true;
        } else if (endsWith(content.name, ".cert")) {
            info.hasCert = true;
        } else if (endsWith(content.name, ".xml")) {
            info.hasXml = true;
        }
    }

    std::fclose(file);
    info.valid = !info.contents.empty();
    if (!info.valid) {
        info.error = "Archive vide";
    }
    return info;
}

} // namespace nxf
