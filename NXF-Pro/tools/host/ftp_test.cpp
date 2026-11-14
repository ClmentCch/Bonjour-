/*
 * NXF Pro - FTP server host test.
 *
 * Runs the production FTP server over a temporary directory and drives it with
 * curl, so the wire protocol is checked by a real client rather than by our own
 * code.
 *
 *   g++ -std=c++17 -O2 -Iinclude -Ivendor -include tools/host/switch_shim.h \
 *       src/ftp_server.cpp src/model.cpp tools/host/ftp_test.cpp -o ftptest -lpthread
 */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "nxf/app.hpp"
#include "nxf/platform_switch.hpp"

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool condition, const std::string& label, const std::string& detail = {}) {
    if (condition) {
        std::printf("PASS  %s\n", label.c_str());
    } else {
        std::printf("FAIL  %s %s\n", label.c_str(), detail.c_str());
        ++failures;
    }
}

std::string readFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << data;
    return static_cast<bool>(stream);
}

/// Runs curl and returns {exitCode, stdout}.
std::pair<int, std::string> curl(const std::string& arguments) {
    const std::string command = "curl " + arguments + " 2>/dev/null";
    std::FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return {-1, {}};
    }
    std::string output;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int status = pclose(pipe);
    return {status, output};
}

class TestStorage final : public nxf::StorageApi {
public:
    explicit TestStorage(std::string root) : root_(std::move(root)) {
        std::error_code ec;
        fs::create_directories(root_, ec);
    }

    bool available() const override { return true; }
    std::string displayName() const override { return "Test"; }
    std::string root() const override { return root_; }

    bool list(const std::string& path, std::vector<nxf::FileEntry>& out,
              std::string& error) override {
        std::error_code ec;
        fs::directory_iterator it(path, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        for (const fs::directory_entry& entry : it) {
            nxf::FileEntry file;
            file.name = entry.path().filename().string();
            std::error_code inner;
            file.isDirectory = entry.is_directory(inner);
            if (!file.isDirectory) {
                file.size = static_cast<std::uint64_t>(entry.file_size(inner));
            }
            const auto stamp = entry.last_write_time(inner);
            file.modified = std::chrono::duration_cast<std::chrono::seconds>(
                                stamp.time_since_epoch())
                                .count();
            out.push_back(file);
        }
        return true;
    }

    bool makeDirectory(const std::string& path, std::string& error) override {
        std::error_code ec;
        if (!fs::create_directory(path, ec)) {
            error = ec.message();
            return false;
        }
        return true;
    }

    bool createFile(const std::string& path, std::string& error) override {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "open";
            return false;
        }
        return true;
    }

    bool renamePath(const std::string& from, const std::string& to, std::string& error) override {
        std::error_code ec;
        fs::rename(from, to, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        return true;
    }

    bool removePath(const std::string& path, std::string& error) override {
        std::error_code ec;
        fs::remove_all(path, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        return true;
    }

    bool copyPath(const std::string& from, const std::string& to, std::string& error) override {
        std::error_code ec;
        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        return true;
    }

    nxf::FileStats stats(const std::string& path) override {
        nxf::FileStats result;
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            return result;
        }
        result.exists = true;
        result.isDirectory = fs::is_directory(path, ec);
        if (!result.isDirectory) {
            result.size = static_cast<std::uint64_t>(fs::file_size(path, ec));
        }
        const auto stamp = fs::last_write_time(path, ec);
        result.modified =
            std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
        return result;
    }

    void space(std::uint64_t& total, std::uint64_t& freeBytes) override {
        total = 0;
        freeBytes = 0;
    }

private:
    std::string root_;
};

constexpr const char* kHost = "127.0.0.1";
constexpr int kPort = 2121;
const std::string kBase = "ftp://switch:nxfpro@127.0.0.1:2121";

} // namespace

int main() {
    const fs::path root = "/tmp/nxf-ftp-root";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "switch", ec);
    fs::create_directories(root / "nand", ec);
    writeFile((root / "switch" / "hello.txt").string(), "Bonjour depuis la Switch\n");
    writeFile((root / "switch" / "payload.bin").string(), std::string(2048, 'x'));

    TestStorage sd(root.string() + "/");
    TestStorage internal((root / "nand").string() + "/");

    auto ftp = nxf::createFtpServer(sd, internal);
    std::string error;
    if (!ftp->start(kPort, "switch", "nxfpro", error)) {
        std::printf("FAIL  start: %s\n", error.c_str());
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    check(ftp->snapshot().running, "serveur démarré");

    // 1. Virtual roots.
    auto listing = curl("-s " + kBase + "/");
    check(listing.second.find("sd") != std::string::npos &&
              listing.second.find("internal") != std::string::npos,
          "LIST / expose sd et internal", listing.second);

    // 2. Directory listing.
    listing = curl("-s " + kBase + "/sd/switch/");
    check(listing.second.find("hello.txt") != std::string::npos,
          "LIST /sd/switch contient hello.txt", listing.second);
    check(listing.second.find("drwxr-xr-x") == std::string::npos, "LIST fichiers typés");

    // 3. Download.
    const auto download = curl("-s " + kBase + "/sd/switch/hello.txt");
    check(download.second == "Bonjour depuis la Switch\n", "RETR contenu exact", download.second);

    // 4. SIZE.
    const auto size = curl("-s " + kBase + "/sd/switch/payload.bin -Q \"SIZE /sd/switch/payload.bin\"");
    check(size.first == 0, "SIZE accepté");

    // 5. Upload.
    writeFile("/tmp/nxf-upload.bin", std::string(5000, 'Z'));
    auto upload = curl("-s -T /tmp/nxf-upload.bin " + kBase + "/sd/switch/upload.bin");
    check(upload.first == 0, "STOR upload");
    check(readFile((root / "switch" / "upload.bin").string()) == std::string(5000, 'Z'),
          "STOR fichier complet");

    // 6. MKD + rename.
    curl("-s " + kBase + "/sd/ -Q \"MKD /sd/nouveau\"");
    check(fs::is_directory(root / "nouveau", ec), "MKD crée le dossier");
    curl("-s " + kBase + "/sd/ -Q \"RNFR /sd/nouveau\" -Q \"RNTO /sd/renomme\"");
    check(fs::is_directory(root / "renomme", ec), "RNFR/RNTO renomme");
    check(!fs::exists(root / "nouveau", ec), "ancien nom supprimé");

    // 7. DELE + RMD.
    curl("-s " + kBase + "/sd/switch/ -Q \"DELE /sd/switch/upload.bin\"");
    check(!fs::exists(root / "switch" / "upload.bin", ec), "DELE supprime le fichier");
    curl("-s " + kBase + "/sd/ -Q \"RMD /sd/renomme\"");
    check(!fs::exists(root / "renomme", ec), "RMD supprime le dossier");

    // 8. Internal memory.
    listing = curl("-s " + kBase + "/internal/");
    check(listing.first == 0, "LIST /internal accessible");

    // 9. Credentials.
    const auto denied = curl("-s -o /dev/null -w \"%{http_code}\" "
                             "ftp://switch:wrong@127.0.0.1:2121/sd/");
    check(denied.second.find("530") != std::string::npos, "mauvais mot de passe refusé",
          denied.second);

    // 10. Path traversal is contained.
    const auto escape = curl("-s " + kBase + "/sd/../../etc/passwd");
    check(escape.second.find("root:") == std::string::npos, "pas d'évasion de répertoire");

    const auto snapshot = ftp->snapshot();
    std::printf("--- journal serveur (%zu lignes) ---\n", snapshot.log.size());
    for (const std::string& line : snapshot.log) {
        std::printf("  %s\n", line.c_str());
    }

    ftp->stop();
    check(!ftp->snapshot().running, "serveur arrêté");

    std::printf("\n%s (%d échec(s))\n", failures == 0 ? "TOUT EST VERT" : "ÉCHECS", failures);
    return failures == 0 ? 0 : 1;
}
