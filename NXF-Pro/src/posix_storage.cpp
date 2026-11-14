/*
 * NXF Pro - real filesystem backend.
 *
 * Both storages are plain devoptab mounts, so the whole explorer is implemented
 * on top of POSIX calls: opendir/readdir for listing, stat for metadata,
 * mkdir/rename/unlink/rmdir for mutations.
 *
 * MIT licensed, see LICENSE.
 */
#include "nxf/platform_switch.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nxf {
namespace {

constexpr std::size_t kCopyBuffer = 128 * 1024;

std::string deviceName(const std::string& root) {
    const std::size_t colon = root.find(':');
    return colon == std::string::npos ? root : root.substr(0, colon);
}

std::string describeError(int code) {
    switch (code) {
        case EACCES: return "Accès refusé";
        case ENOENT: return "Élément introuvable";
        case EEXIST: return "Cet élément existe déjà";
        case ENOSPC: return "Espace insuffisant";
        case ENOTDIR: return "Ce n'est pas un dossier";
        case EISDIR: return "C'est un dossier";
        case EROFS: return "Système de fichiers en lecture seule";
        case ENAMETOOLONG: return "Nom trop long";
        case EBUSY: return "Fichier utilisé par un autre programme";
        default: return "Erreur fichier (" + std::to_string(code) + ")";
    }
}

std::string lowerCopy(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

bool removeRecursive(const std::string& path, std::string& error) {
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) {
        error = describeError(errno);
        return false;
    }

    if (!S_ISDIR(info.st_mode)) {
        if (unlink(path.c_str()) != 0) {
            error = describeError(errno);
            return false;
        }
        return true;
    }

    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        error = describeError(errno);
        return false;
    }
    std::string failure;
    while (struct dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        const std::string child = joinPath(path, entry->d_name);
        if (!removeRecursive(child, failure)) {
            closedir(dir);
            error = failure;
            return false;
        }
    }
    closedir(dir);

    if (rmdir(path.c_str()) != 0) {
        error = describeError(errno);
        return false;
    }
    return true;
}

bool copyFile(const std::string& from, const std::string& to, std::string& error) {
    std::FILE* source = std::fopen(from.c_str(), "rb");
    if (source == nullptr) {
        error = describeError(errno);
        return false;
    }
    std::FILE* target = std::fopen(to.c_str(), "wb");
    if (target == nullptr) {
        error = describeError(errno);
        std::fclose(source);
        return false;
    }

    std::string buffer(kCopyBuffer, '\0');
    bool ok = true;
    for (;;) {
        const std::size_t read = std::fread(&buffer[0], 1, buffer.size(), source);
        if (read > 0 && std::fwrite(buffer.data(), 1, read, target) != read) {
            error = describeError(errno);
            ok = false;
            break;
        }
        if (read < buffer.size()) {
            if (std::ferror(source) != 0) {
                error = describeError(errno);
                ok = false;
            }
            break;
        }
    }
    std::fclose(source);
    std::fclose(target);

    if (!ok) {
        unlink(to.c_str());
    }
    return ok;
}

bool copyRecursive(const std::string& from, const std::string& to, std::string& error) {
    struct stat info {};
    if (stat(from.c_str(), &info) != 0) {
        error = describeError(errno);
        return false;
    }

    if (!S_ISDIR(info.st_mode)) {
        return copyFile(from, to, error);
    }

    if (mkdir(to.c_str(), 0777) != 0 && errno != EEXIST) {
        error = describeError(errno);
        return false;
    }

    DIR* dir = opendir(from.c_str());
    if (dir == nullptr) {
        error = describeError(errno);
        return false;
    }
    std::string failure;
    while (struct dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!copyRecursive(joinPath(from, entry->d_name), joinPath(to, entry->d_name), failure)) {
            closedir(dir);
            error = failure;
            return false;
        }
    }
    closedir(dir);
    return true;
}

class PosixStorage final : public StorageApi {
public:
    PosixStorage(std::string root, std::string displayName)
        : root_(std::move(root)), display_(std::move(displayName)) {
        DIR* dir = opendir(root_.c_str());
        available_ = dir != nullptr;
        if (dir != nullptr) {
            closedir(dir);
        }
    }

    bool available() const override { return available_; }
    std::string displayName() const override { return display_; }
    std::string root() const override { return root_; }

    bool list(const std::string& path, std::vector<FileEntry>& out, std::string& error) override {
        out.clear();
        if (!allowed(path)) {
            error = "Chemin invalide";
            return false;
        }

        DIR* dir = opendir(path.c_str());
        if (dir == nullptr) {
            error = describeError(errno);
            available_ = false;
            return false;
        }
        available_ = true;

        while (struct dirent* entry = readdir(dir)) {
            if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            FileEntry file;
            file.name = entry->d_name;
            struct stat info {};
            if (stat(joinPath(path, file.name).c_str(), &info) == 0) {
                file.isDirectory = S_ISDIR(info.st_mode) != 0;
                file.size = file.isDirectory ? 0 : static_cast<std::uint64_t>(info.st_size);
                file.modified = static_cast<std::int64_t>(info.st_mtime);
            } else {
                file.isDirectory = false;
            }
            out.push_back(std::move(file));
        }
        closedir(dir);
        return true;
    }

    bool makeDirectory(const std::string& path, std::string& error) override {
        if (!allowed(path)) {
            error = "Chemin invalide";
            return false;
        }
        if (mkdir(path.c_str(), 0777) != 0) {
            error = describeError(errno);
            return false;
        }
        return true;
    }

    bool createFile(const std::string& path, std::string& error) override {
        if (!allowed(path)) {
            error = "Chemin invalide";
            return false;
        }
        std::FILE* file = std::fopen(path.c_str(), "wb");
        if (file == nullptr) {
            error = describeError(errno);
            return false;
        }
        std::fclose(file);
        return true;
    }

    bool renamePath(const std::string& from, const std::string& to, std::string& error) override {
        if (!allowed(from) || !allowed(to)) {
            error = "Chemin invalide";
            return false;
        }
        if (rename(from.c_str(), to.c_str()) != 0) {
            error = describeError(errno);
            return false;
        }
        return true;
    }

    bool removePath(const std::string& path, std::string& error) override {
        if (!allowed(path)) {
            error = "Chemin invalide";
            return false;
        }
        return removeRecursive(path, error);
    }

    bool copyPath(const std::string& from, const std::string& to, std::string& error) override {
        if (!allowed(from) || !allowed(to)) {
            error = "Chemin invalide";
            return false;
        }
        return copyRecursive(from, to, error);
    }

    FileStats stats(const std::string& path) override {
        FileStats result;
        struct stat info {};
        if (stat(path.c_str(), &info) != 0) {
            return result;
        }
        result.exists = true;
        result.isDirectory = S_ISDIR(info.st_mode) != 0;
        result.size = result.isDirectory ? 0 : static_cast<std::uint64_t>(info.st_size);
        result.modified = static_cast<std::int64_t>(info.st_mtime);
        return result;
    }

    void space(std::uint64_t& total, std::uint64_t& freeBytes) override {
        total = 0;
        freeBytes = 0;
#ifdef __SWITCH__
        FsFileSystem* fs = fsdevGetDeviceFileSystem(deviceName(root_).c_str());
        if (fs == nullptr) {
            return;
        }
        s64 value = 0;
        if (R_SUCCEEDED(fsFsGetTotalSpace(fs, "/", &value)) && value > 0) {
            total = static_cast<std::uint64_t>(value);
        }
        if (R_SUCCEEDED(fsFsGetFreeSpace(fs, "/", &value)) && value > 0) {
            freeBytes = static_cast<std::uint64_t>(value);
        }
#endif
    }

private:
    bool allowed(const std::string& path) const {
        if (path.size() < root_.size()) {
            return false;
        }
        return lowerCopy(path.substr(0, root_.size())) == lowerCopy(root_);
    }

    std::string root_;
    std::string display_;
    bool available_ = false;
};

} // namespace

std::unique_ptr<StorageApi> openSdStorage() {
    return std::unique_ptr<StorageApi>(new PosixStorage("sdmc:/", "Carte SD"));
}

std::unique_ptr<StorageApi> openInternalStorage() {
    return std::unique_ptr<StorageApi>(new PosixStorage("nand:/", "Mémoire interne"));
}

} // namespace nxf
