/*
 * NXF Pro - installed application management.
 *
 * Lists every installed title through the console's "ns" service, reads its
 * NACP (name, publisher, icon) and can erase a title completely:
 *
 *   - the application, its updates and its DLC (nsDeleteApplicationCompletely)
 *   - every save file belonging to it, on NAND and on the SD card
 *   - the Atmosphère content folders left behind on the SD card
 *
 * MIT licensed, see LICENSE.
 */
#include "nxf/platform_switch.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <switch.h>

namespace nxf {
namespace {

constexpr const char* kAtmosphereRoots[] = {"sdmc:/atmosphere/contents/",
                                            "sdmc:/atmosphere/titles/"};

std::string lowerCase(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

bool removeRecursive(const std::string& path) {
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    if (!S_ISDIR(info.st_mode)) {
        return unlink(path.c_str()) == 0;
    }

    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        return false;
    }
    std::vector<std::string> children;
    while (struct dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        children.emplace_back(path + "/" + entry->d_name);
    }
    closedir(dir);

    bool ok = true;
    for (const std::string& child : children) {
        ok = removeRecursive(child) && ok;
    }
    return rmdir(path.c_str()) == 0 && ok;
}

/// Picks the best available NACP name: French first, then the console default,
/// then any non-empty entry.
std::string nacpName(const NacpStruct& nacp) {
    if (nacp.lang[3].name[0] != '\0') {
        return nacp.lang[3].name;
    }
    if (nacp.lang[0].name[0] != '\0') {
        return nacp.lang[0].name;
    }
    for (const NacpLanguageEntry& entry : nacp.lang) {
        if (entry.name[0] != '\0') {
            return entry.name;
        }
    }
    return std::string();
}

std::string nacpAuthor(const NacpStruct& nacp) {
    if (nacp.lang[3].author[0] != '\0') {
        return nacp.lang[3].author;
    }
    if (nacp.lang[0].author[0] != '\0') {
        return nacp.lang[0].author;
    }
    return std::string();
}

std::string formatVersion(std::uint32_t version) {
    if (version == 0) {
        return std::string();
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "v%u.%u.%u", (version >> 16) & 0xFFFFu,
                  (version >> 8) & 0xFFu, version & 0xFFu);
    return std::string(buffer);
}

class NsTitles final : public TitleApi {
public:
    NsTitles() { initialized_ = R_SUCCEEDED(nsInitialize()); }

    ~NsTitles() override {
        if (initialized_) {
            nsExit();
        }
    }

    bool available() const override { return initialized_; }

    bool list(std::vector<TitleEntry>& out, std::string& error) override {
        out.clear();
        if (!initialized_) {
            error = "Service ns indisponible";
            return false;
        }

        s32 total = 0;
        std::vector<NsApplicationRecord> records(512);
        Result rc = nsListApplicationRecord(records.data(), static_cast<s32>(records.size()), 0,
                                            &total);
        if (R_FAILED(rc)) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "Lecture des titres refusée (0x%08X)", rc);
            error = buffer;
            return false;
        }
        if (total > static_cast<s32>(records.size())) {
            records.resize(static_cast<std::size_t>(total) + 32);
            rc = nsListApplicationRecord(records.data(), static_cast<s32>(records.size()), 0, &total);
            if (R_FAILED(rc)) {
                error = "Lecture des titres refusée";
                return false;
            }
        }

        // NsApplicationControlData is ~144 KiB, keep it off the stack.
        std::vector<std::uint8_t> control(sizeof(NsApplicationControlData) + 0x1000);
        auto* data = reinterpret_cast<NsApplicationControlData*>(control.data());

        for (s32 i = 0; i < total; ++i) {
            const u64 id = records[static_cast<std::size_t>(i)].application_id;
            if (id == 0) {
                continue;
            }

            TitleEntry entry;
            entry.id = id;

            std::memset(data, 0, control.size());
            u64 actual = 0;
            Result info = nsGetApplicationControlData(NsApplicationControlSource_Storage, id, data,
                                                      control.size(), &actual);
            if (R_FAILED(info)) {
                info = nsGetApplicationControlData(NsApplicationControlSource_CacheOnly, id, data,
                                                   control.size(), &actual);
            }
            if (R_SUCCEEDED(info)) {
                entry.name = nacpName(data->nacp);
                entry.author = nacpAuthor(data->nacp);

                constexpr std::size_t kIconOffset = offsetof(NsApplicationControlData, icon);
                if (actual > kIconOffset) {
                    const std::size_t size = std::min<std::size_t>(
                        static_cast<std::size_t>(actual) - kIconOffset, sizeof(data->icon));
                    if (size > 2) {
                        entry.iconJpeg.assign(data->icon, data->icon + size);
                    }
                }
            }

            NsApplicationContentMetaStatus meta[32];
            s32 metaCount = 0;
            if (R_SUCCEEDED(nsListApplicationContentMetaStatus(id, 0, meta, 32, &metaCount))) {
                for (s32 m = 0; m < metaCount && m < 32; ++m) {
                    switch (meta[m].meta_type) {
                        case NcmContentMetaType_Application:
                            if (entry.version.empty()) {
                                entry.version = formatVersion(meta[m].version);
                            }
                            break;
                        case NcmContentMetaType_Patch:
                            ++entry.updates;
                            entry.version = formatVersion(meta[m].version);
                            break;
                        case NcmContentMetaType_AddOnContent:
                            ++entry.dlc;
                            break;
                        default:
                            break;
                    }
                }
            }

            if (entry.name.empty()) {
                char fallback[48];
                std::snprintf(fallback, sizeof(fallback), "Titre %016llX",
                              static_cast<unsigned long long>(id));
                entry.name = fallback;
            }

            out.push_back(std::move(entry));
        }

        std::sort(out.begin(), out.end(), [](const TitleEntry& a, const TitleEntry& b) {
            return lowerCase(a.name) < lowerCase(b.name);
        });
        return true;
    }

    bool remove(std::uint64_t id, TitleRemovalReport& report, std::string& error) override {
        if (!initialized_) {
            error = "Service ns indisponible";
            return false;
        }

        const Result rc = nsDeleteApplicationCompletely(id);
        if (R_FAILED(rc)) {
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer),
                          "Suppression refusée (0x%08X) — mode application requis", rc);
            error = buffer;
            return false;
        }

        report.contentRemoved = true;
        report.savesRemoved = deleteSaves(id);
        report.leftoversRemoved = deleteLeftovers(id);
        return true;
    }

private:
    int deleteSaves(std::uint64_t id) {
        int removed = 0;
        const FsSaveDataSpaceId spaces[] = {FsSaveDataSpaceId_User, FsSaveDataSpaceId_SdUser};

        for (const FsSaveDataSpaceId space : spaces) {
            FsSaveDataInfoReader reader;
            if (R_FAILED(fsOpenSaveDataInfoReader(&reader, space))) {
                continue;
            }

            FsSaveDataInfo info {};
            s64 read = 0;
            while (R_SUCCEEDED(fsSaveDataInfoReaderRead(&reader, &info, 1, &read)) && read > 0) {
                if (info.application_id != id) {
                    continue;
                }
                if (R_SUCCEEDED(fsDeleteSaveDataFileSystemBySaveDataSpaceId(
                        static_cast<FsSaveDataSpaceId>(info.save_data_space_id),
                        info.save_data_id))) {
                    ++removed;
                }
            }
            fsSaveDataInfoReaderClose(&reader);
        }
        return removed;
    }

    int deleteLeftovers(std::uint64_t id) {
        char suffix[24];
        std::snprintf(suffix, sizeof(suffix), "%016llx", static_cast<unsigned long long>(id));

        int removed = 0;
        for (const char* root : kAtmosphereRoots) {
            const std::string path = std::string(root) + suffix;
            struct stat info {};
            if (stat(path.c_str(), &info) != 0) {
                continue;
            }
            if (removeRecursive(path)) {
                ++removed;
            }
        }
        return removed;
    }

    bool initialized_ = false;
};

} // namespace

std::unique_ptr<TitleApi> createTitleApi() {
    return std::unique_ptr<TitleApi>(new NsTitles());
}

} // namespace nxf
