/*
 * NXF Pro - built-in FTP server.
 *
 * A real FTP implementation (RFC 959 compatible subset used by Windows
 * Explorer, macOS Finder, FileZilla, lftp, curl...) running on its own thread:
 * passive data channel, full CRUD, LIST/SIZE/MDTM, UTF-8 names.
 *
 * The server exposes two virtual roots:
 *   /sd        -> the SD card (sdmc:)
 *   /internal  -> the user NAND partition, when it could be mounted
 *
 * MIT licensed, see LICENSE.
 */
#include "nxf/platform_switch.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nxf {
namespace {

constexpr std::size_t kLogLimit = 140;
constexpr std::size_t kMaxCommand = 4096;
constexpr int kSelectTimeoutMs = 200;
constexpr int kDataTimeoutMs = 15000;

std::string upperCase(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return text;
}

/// Textual normalisation of a virtual path ("/sd/../sd/switch" -> "/sd/switch").
std::string normalizeVirtual(const std::string& input) {
    std::vector<std::string> parts;
    std::string current;
    const auto flush = [&parts, &current]() {
        if (current.empty() || current == ".") {
            // nothing to do
        } else if (current == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else {
            parts.push_back(current);
        }
        current.clear();
    };
    for (char c : input) {
        if (c == '/') {
            flush();
        } else {
            current.push_back(c);
        }
    }
    flush();

    std::string out;
    for (const std::string& part : parts) {
        out += "/";
        out += part;
    }
    return out.empty() ? "/" : out;
}

std::string parentOf(const std::string& path) {
    if (path.size() <= 1) {
        return "/";
    }
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
        return "/";
    }
    return path.substr(0, slash);
}

std::string nameOf(const std::string& path) {
    if (path.size() <= 1) {
        return path;
    }
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string listDate(std::int64_t epoch) {
    static const char* kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = 0;
    int day = 1;
    int year = 1970;
    if (epoch > 0) {
        std::time_t raw = static_cast<std::time_t>(epoch);
        std::tm* parts = std::localtime(&raw);
        if (parts != nullptr) {
            month = parts->tm_mon % 12;
            day = parts->tm_mday;
            year = parts->tm_year + 1900;
        }
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s %2d  %04d", kMonths[month], day, year);
    return std::string(buffer);
}

std::string addressText(const sockaddr_in& address) {
    const char* text = inet_ntoa(address.sin_addr);
    return text == nullptr ? std::string("0.0.0.0") : std::string(text);
}

/// Local interface address, discovered without sending a single packet.
std::string detectLocalIp() {
    const int fd = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
    if (fd >= 0) {
        sockaddr_in target {};
        target.sin_family = AF_INET;
        target.sin_port = htons(53);
        target.sin_addr.s_addr = htonl(0x08080808u);
        if (connect(fd, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0) {
            sockaddr_in local {};
            socklen_t length = sizeof(local);
            if (getsockname(fd, reinterpret_cast<sockaddr*>(&local), &length) == 0 &&
                local.sin_addr.s_addr != 0) {
                const std::string result = addressText(local);
                close(fd);
                return result;
            }
        }
        close(fd);
    }
#ifdef __SWITCH__
    std::uint32_t ip = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", (ip >> 24) & 0xFFu,
                      (ip >> 16) & 0xFFu, (ip >> 8) & 0xFFu, ip & 0xFFu);
        return std::string(buffer);
    }
#endif
    return "127.0.0.1";
}

} // namespace

class FtpServerImpl final : public FtpController {
public:
    FtpServerImpl(StorageApi& sd, StorageApi& internal) : sd_(sd), internal_(internal) {}

    ~FtpServerImpl() override { stop(); }

    bool start(int port, const std::string& user, const std::string& password,
               std::string& error) override {
        if (running_.load()) {
            error = "Le serveur tourne déjà";
            return false;
        }

        user_ = user;
        password_ = password;

        int fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (fd < 0) {
            error = "Impossible d'ouvrir le socket";
            return false;
        }
        int reuse = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(port));
        address.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            close(fd);
            error = "Port " + std::to_string(port) + " indisponible";
            return false;
        }
        if (listen(fd, 4) < 0) {
            close(fd);
            error = "Écoute impossible";
            return false;
        }

        listenFd_ = fd;
        running_.store(true);
        advertisedIp_ = detectLocalIp();
        address_ = advertisedIp_ + ":" + std::to_string(port);

        if (R_FAILED(threadCreate(&thread_, &FtpServerImpl::threadEntry, this, stack_,
                                  sizeof(stack_), 0x2C, -2))) {
            running_.store(false);
            close(fd);
            listenFd_ = -1;
            error = "Thread FTP impossible";
            return false;
        }
        threadStarted_ = true;
        if (R_FAILED(threadStart(&thread_))) {
            threadStarted_ = false;
            running_.store(false);
            close(fd);
            listenFd_ = -1;
            error = "Démarrage du thread FTP impossible";
            return false;
        }

        logLine("Serveur démarré sur " + address_);
        return true;
    }

    void stop() override {
        const bool wasRunning = running_.exchange(false);
        if (!wasRunning && listenFd_ < 0) {
            return;
        }

        if (listenFd_ >= 0) {
            shutdown(listenFd_, SHUT_RDWR);
        }
        const int client = clientFd_.load();
        if (client >= 0) {
            shutdown(client, SHUT_RDWR);
        }

        if (threadStarted_) {
            threadWaitForExit(&thread_);
            threadClose(&thread_);
            threadStarted_ = false;
        }

        if (listenFd_ >= 0) {
            close(listenFd_);
            listenFd_ = -1;
        }
        clientFd_.store(-1);
        clients_.store(0);
        if (wasRunning) {
            logLine("Serveur arrêté");
        }
    }

    FtpSnapshot snapshot() const override {
        FtpSnapshot state;
        state.running = running_.load();
        state.clients = clients_.load();
        state.address = address_;
        std::lock_guard<std::mutex> lock(logMutex_);
        state.log = log_;
        return state;
    }

private:
    static void threadEntry(void* arg) {
        static_cast<FtpServerImpl*>(arg)->run();
    }

    void logLine(const std::string& text) {
        std::lock_guard<std::mutex> lock(logMutex_);
        log_.push_back(text);
        if (log_.size() > kLogLimit) {
            log_.erase(log_.begin());
        }
    }

    /// Address the client is actually talking to, so PASV always works even
    /// when the console has several interfaces.
    std::string currentIp() const {
        if (control_ >= 0) {
            sockaddr_in local {};
            socklen_t length = sizeof(local);
            if (getsockname(control_, reinterpret_cast<sockaddr*>(&local), &length) == 0 &&
                local.sin_addr.s_addr != 0) {
                return addressText(local);
            }
        }
        return advertisedIp_.empty() ? std::string("127.0.0.1") : advertisedIp_;
    }

    // ---------------------------------------------------------------- network

    void run() {
        while (running_.load()) {
            fd_set set;
            FD_ZERO(&set);
            const int listen = listenFd_;
            if (listen < 0) {
                break;
            }
            FD_SET(listen, &set);
            timeval timeout {};
            timeout.tv_sec = 0;
            timeout.tv_usec = kSelectTimeoutMs * 1000;
            const int ready = select(listen + 1, &set, nullptr, nullptr, &timeout);
            if (!running_.load()) {
                break;
            }
            if (ready <= 0) {
                continue;
            }

            sockaddr_in peer {};
            socklen_t peerLength = sizeof(peer);
            const int fd = static_cast<int>(
                accept(listen, reinterpret_cast<sockaddr*>(&peer), &peerLength));
            if (fd < 0) {
                continue;
            }

            clientFd_.store(fd);
            clients_.store(1);
            serveClient(fd, addressText(peer));
            clients_.store(0);
            clientFd_.store(-1);
            close(fd);
        }
    }

    void serveClient(int fd, const std::string& peer) {
        control_ = fd;
        cwd_ = "/";
        authed_ = false;
        userGiven_.clear();
        renameFrom_.clear();
        closePassive();

        timeval timeout {};
        timeout.tv_sec = 0;
        timeout.tv_usec = 250 * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        peer_ = peer;
        reply(220, "NXF Pro - serveur FTP prêt");
        logLine("Connexion de " + peer);

        std::string line;
        bool quit = false;
        while (running_.load() && !quit) {
            if (!readLine(line)) {
                break;
            }
            if (line.empty()) {
                continue;
            }
            quit = handleCommand(line);
        }

        closePassive();
        control_ = -1;
        logLine("Déconnexion de " + peer);
    }

    bool readLine(std::string& out) {
        out.clear();
        for (;;) {
            if (!running_.load()) {
                return false;
            }
            char c = 0;
            const ssize_t got = recv(control_, &c, 1, 0);
            if (got == 0) {
                return false;
            }
            if (got < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    continue;
                }
                return false;
            }
            if (c == '\n') {
                return true;
            }
            if (c != '\r') {
                out.push_back(c);
            }
            if (out.size() > kMaxCommand) {
                return false;
            }
        }
    }

    void reply(int code, const std::string& text) {
        const std::string data = std::to_string(code) + " " + text + "\r\n";
        sendRaw(control_, data.data(), data.size());
    }

    void replyText(const std::string& text) { sendRaw(control_, text.data(), text.size()); }

    void sendRaw(int fd, const char* data, std::size_t size) {
        std::size_t sent = 0;
        while (sent < size && running_.load()) {
            const ssize_t chunk = send(fd, data + sent, size - sent, 0);
            if (chunk <= 0) {
                if (chunk < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                    continue;
                }
                return;
            }
            sent += static_cast<std::size_t>(chunk);
        }
    }

    // ----------------------------------------------------------- data channel

    void closePassive() {
        if (pasvFd_ >= 0) {
            close(pasvFd_);
            pasvFd_ = -1;
        }
    }

    bool openPassive(int& port) {
        closePassive();
        const int fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (fd < 0) {
            return false;
        }
        int reuse = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_port = 0;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
            listen(fd, 1) < 0) {
            close(fd);
            return false;
        }

        sockaddr_in bound {};
        socklen_t length = sizeof(bound);
        if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &length) < 0) {
            close(fd);
            return false;
        }
        pasvFd_ = fd;
        port = ntohs(bound.sin_port);
        return true;
    }

    int acceptData() {
        if (pasvFd_ < 0) {
            return -1;
        }
        const int deadline = kDataTimeoutMs / kSelectTimeoutMs;
        for (int attempt = 0; attempt < deadline && running_.load(); ++attempt) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(pasvFd_, &set);
            timeval timeout {};
            timeout.tv_sec = 0;
            timeout.tv_usec = kSelectTimeoutMs * 1000;
            const int ready = select(pasvFd_ + 1, &set, nullptr, nullptr, &timeout);
            if (ready > 0) {
                sockaddr_in peer {};
                socklen_t length = sizeof(peer);
                const int fd = static_cast<int>(
                    accept(pasvFd_, reinterpret_cast<sockaddr*>(&peer), &length));
                closePassive();
                return fd;
            }
        }
        closePassive();
        return -1;
    }

    // --------------------------------------------------------- virtual paths

    std::string virtualPath(const std::string& argument) const {
        if (argument.empty()) {
            return cwd_;
        }
        if (argument[0] == '/') {
            return normalizeVirtual(argument);
        }
        if (cwd_ == "/") {
            return normalizeVirtual("/" + argument);
        }
        return normalizeVirtual(cwd_ + "/" + argument);
    }

    /// Maps a virtual path to a real path. Returns false for unknown roots.
    bool resolveVirtual(const std::string& virtualName, std::string& real) const {
        if (virtualName == "/") {
            real.clear();
            return true;
        }
        std::string rest = virtualName.substr(1);
        const std::size_t slash = rest.find('/');
        const std::string head = slash == std::string::npos ? rest : rest.substr(0, slash);
        const std::string tail = slash == std::string::npos ? std::string() : rest.substr(slash + 1);

        if (head == "sd") {
            real = tail.empty() ? sd_.root() : sd_.root() + tail;
            return true;
        }
        if (head == "internal" && internal_.available()) {
            real = tail.empty() ? internal_.root() : internal_.root() + tail;
            return true;
        }
        return false;
    }

    bool listVirtual(const std::string& virtualName, std::vector<FileEntry>& out,
                     std::string& error) {
        if (virtualName == "/") {
            out.clear();
            FileEntry sd;
            sd.name = "sd";
            sd.isDirectory = true;
            out.push_back(sd);
            if (internal_.available()) {
                FileEntry internal;
                internal.name = "internal";
                internal.isDirectory = true;
                out.push_back(internal);
            }
            return true;
        }

        std::string real;
        if (!resolveVirtual(virtualName, real) || real.empty()) {
            error = "Chemin inconnu";
            return false;
        }
        StorageApi& api = (virtualName.rfind("/internal", 0) == 0) ? internal_ : sd_;
        return api.list(real, out, error);
    }

    StorageApi& apiFor(const std::string& virtualName) {
        return virtualName.rfind("/internal", 0) == 0 ? internal_ : sd_;
    }

    std::string formatListing(const std::vector<FileEntry>& entries) const {
        std::string listing;
        for (const FileEntry& entry : entries) {
            char head[128];
            std::snprintf(head, sizeof(head), "%s 1 switch nxfpro %12llu %s ",
                          entry.isDirectory ? "drwxr-xr-x" : "-rw-r--r--",
                          static_cast<unsigned long long>(entry.isDirectory ? 0 : entry.size),
                          listDate(entry.modified).c_str());
            listing += head;
            listing += entry.name;
            listing += "\r\n";
        }
        return listing;
    }

    // -------------------------------------------------------------- commands

    bool handleCommand(const std::string& line) {
        const std::size_t space = line.find(' ');
        const std::string verb = upperCase(line.substr(0, space));
        std::string argument =
            space == std::string::npos ? std::string() : line.substr(space + 1);
        while (!argument.empty() && argument.front() == ' ') {
            argument.erase(argument.begin());
        }

        if (verb == "USER") {
            userGiven_ = argument;
            authed_ = false;
            reply(331, "Mot de passe requis");
            return false;
        }
        if (verb == "PASS") {
            if (user_ == userGiven_ && password_ == argument) {
                authed_ = true;
                reply(230, "Connecté, bienvenue");
                logLine("Utilisateur " + userGiven_ + " authentifié");
            } else {
                reply(530, "Identifiants refusés");
                logLine("Échec d'authentification");
            }
            return false;
        }
        if (verb == "QUIT") {
            reply(221, "À bientôt");
            return true;
        }
        if (verb == "NOOP") {
            reply(200, "OK");
            return false;
        }
        if (verb == "SYST") {
            reply(215, "UNIX Type: L8");
            return false;
        }
        if (verb == "FEAT") {
            replyText("211-Fonctionnalités:\r\n UTF8\r\n SIZE\r\n MDTM\r\n PASV\r\n"
                      " REST STREAM\r\n211 Fin\r\n");
            return false;
        }
        if (verb == "OPTS") {
            reply(200, "Option acceptée");
            return false;
        }
        if (verb == "TYPE" || verb == "MODE" || verb == "STRU") {
            reply(200, "Mode accepté");
            return false;
        }
        if (verb == "PWD" || verb == "XPWD") {
            reply(257, "\"" + cwd_ + "\" est le dossier courant");
            return false;
        }

        if (!authed_) {
            reply(530, "Authentification requise");
            return false;
        }

        if (verb == "CWD" || verb == "XCWD") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real)) {
                reply(550, "Dossier introuvable");
                return false;
            }
            if (!real.empty()) {
                FileStats stats = apiFor(target).stats(real);
                if (!stats.exists || !stats.isDirectory) {
                    reply(550, "Dossier introuvable");
                    return false;
                }
            }
            cwd_ = target;
            reply(250, "Dossier courant: " + cwd_);
            return false;
        }
        if (verb == "CDUP" || verb == "XCUP") {
            cwd_ = parentOf(cwd_);
            reply(250, "Dossier courant: " + cwd_);
            return false;
        }
        if (verb == "PASV") {
            int port = 0;
            if (!openPassive(port)) {
                reply(425, "Canal passif impossible");
                return false;
            }
            const std::string ip = currentIp();
            int a = 0, b = 0, c = 0, d = 0;
            std::sscanf(ip.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d);
            char message[96];
            std::snprintf(message, sizeof(message), "Mode passif (%d,%d,%d,%d,%d,%d)", a, b, c, d,
                          port / 256, port % 256);
            reply(227, message);
            logLine("PASV " + std::to_string(port));
            return false;
        }
        if (verb == "PORT" || verb == "EPRT") {
            reply(502, "Mode actif non supporté, utilisez PASV");
            return false;
        }
        if (verb == "LIST" || verb == "NLST") {
            const std::string target = virtualPath(stripOptions(argument));
            std::vector<FileEntry> entries;
            std::string error;
            if (!listVirtual(target, entries, error)) {
                reply(550, error.empty() ? "Dossier introuvable" : error);
                return false;
            }
            const int data = acceptData();
            if (data < 0) {
                reply(425, "Connexion de données absente");
                return false;
            }
            reply(150, "Transfert du listing");
            std::string payload;
            if (verb == "NLST") {
                for (const FileEntry& entry : entries) {
                    payload += entry.name;
                    payload += "\r\n";
                }
            } else {
                payload = formatListing(entries);
            }
            sendRaw(data, payload.data(), payload.size());
            close(data);
            reply(226, "Transfert terminé");
            logLine(verb + " " + target + " (" + std::to_string(entries.size()) + ")");
            return false;
        }
        if (verb == "RETR") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Fichier introuvable");
                return false;
            }
            FileStats stats = apiFor(target).stats(real);
            if (!stats.exists || stats.isDirectory) {
                reply(550, "Fichier introuvable");
                return false;
            }
            std::FILE* file = std::fopen(real.c_str(), "rb");
            if (file == nullptr) {
                reply(550, "Lecture impossible");
                return false;
            }
            const int data = acceptData();
            if (data < 0) {
                std::fclose(file);
                reply(425, "Connexion de données absente");
                return false;
            }
            reply(150, "Envoi de " + nameOf(target));
            std::string buffer(64 * 1024, '\0');
            for (;;) {
                const std::size_t read = std::fread(&buffer[0], 1, buffer.size(), file);
                if (read > 0) {
                    sendRaw(data, buffer.data(), read);
                }
                if (read < buffer.size()) {
                    break;
                }
            }
            std::fclose(file);
            close(data);
            reply(226, "Transfert terminé");
            logLine("RETR " + target);
            return false;
        }
        if (verb == "STOR" || verb == "APPE") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Chemin invalide");
                return false;
            }
            const int data = acceptData();
            if (data < 0) {
                reply(425, "Connexion de données absente");
                return false;
            }
            std::FILE* file = std::fopen(real.c_str(), verb == "APPE" ? "ab" : "wb");
            if (file == nullptr) {
                close(data);
                reply(550, "Écriture impossible");
                return false;
            }
            reply(150, "Réception de " + nameOf(target));
            std::string buffer(64 * 1024, '\0');
            for (;;) {
                const ssize_t got = recv(data, &buffer[0], buffer.size(), 0);
                if (got <= 0) {
                    if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                        continue;
                    }
                    break;
                }
                std::fwrite(buffer.data(), 1, static_cast<std::size_t>(got), file);
            }
            std::fclose(file);
            close(data);
            reply(226, "Transfert terminé");
            logLine("STOR " + target);
            return false;
        }
        if (verb == "SIZE") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Fichier introuvable");
                return false;
            }
            FileStats stats = apiFor(target).stats(real);
            if (!stats.exists || stats.isDirectory) {
                reply(550, "Fichier introuvable");
                return false;
            }
            reply(213, std::to_string(stats.size));
            return false;
        }
        if (verb == "MDTM") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Fichier introuvable");
                return false;
            }
            FileStats stats = apiFor(target).stats(real);
            if (!stats.exists) {
                reply(550, "Fichier introuvable");
                return false;
            }
            std::time_t raw = static_cast<std::time_t>(stats.modified);
            std::tm* parts = std::gmtime(&raw);
            if (parts == nullptr) {
                reply(550, "Date inconnue");
                return false;
            }
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", parts);
            reply(213, buffer);
            return false;
        }
        if (verb == "MKD" || verb == "XMKD") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Chemin invalide");
                return false;
            }
            std::string error;
            if (!apiFor(target).makeDirectory(real, error)) {
                reply(550, error);
                return false;
            }
            reply(257, "\"" + target + "\" créé");
            logLine("MKD " + target);
            return false;
        }
        if (verb == "RMD" || verb == "XRMD") {
            const std::string target = virtualPath(argument);
            if (target == "/" || target == "/sd" || target == "/internal") {
                reply(550, "Racine protégée");
                return false;
            }
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Dossier introuvable");
                return false;
            }
            StorageApi& api = apiFor(target);
            FileStats stats = api.stats(real);
            if (!stats.exists || !stats.isDirectory) {
                reply(550, "Dossier introuvable");
                return false;
            }
            std::string error;
            if (!api.removePath(real, error)) {
                reply(550, error);
                return false;
            }
            reply(250, "Dossier supprimé");
            logLine("RMD " + target);
            return false;
        }
        if (verb == "DELE") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Fichier introuvable");
                return false;
            }
            StorageApi& api = apiFor(target);
            FileStats stats = api.stats(real);
            if (!stats.exists || stats.isDirectory) {
                reply(550, "Fichier introuvable");
                return false;
            }
            std::string error;
            if (!api.removePath(real, error)) {
                reply(550, error);
                return false;
            }
            reply(250, "Fichier supprimé");
            logLine("DELE " + target);
            return false;
        }
        if (verb == "RNFR") {
            const std::string target = virtualPath(argument);
            std::string real;
            if (!resolveVirtual(target, real) || real.empty()) {
                reply(550, "Élément introuvable");
                return false;
            }
            renameFrom_ = target;
            reply(350, "Renommage: précisez le nouveau nom");
            return false;
        }
        if (verb == "RNTO") {
            if (renameFrom_.empty()) {
                reply(503, "Aucun renommage en cours");
                return false;
            }
            const std::string target = virtualPath(argument);
            std::string realFrom;
            std::string realTo;
            if (!resolveVirtual(renameFrom_, realFrom) || !resolveVirtual(target, realTo) ||
                realFrom.empty() || realTo.empty()) {
                reply(550, "Chemin invalide");
                return false;
            }
            std::string error;
            if (!apiFor(renameFrom_).renamePath(realFrom, realTo, error)) {
                reply(550, error);
                renameFrom_.clear();
                return false;
            }
            logLine("RNFR " + renameFrom_ + " -> " + target);
            renameFrom_.clear();
            reply(250, "Renommé");
            return false;
        }
        if (verb == "REST") {
            reply(502, "Reprise non supportée");
            return false;
        }

        reply(502, "Commande non implémentée");
        return false;
    }

    static std::string stripOptions(const std::string& argument) {
        std::size_t start = 0;
        while (start < argument.size() && argument[start] == '-') {
            while (start < argument.size() && argument[start] != ' ') {
                ++start;
            }
            while (start < argument.size() && argument[start] == ' ') {
                ++start;
            }
        }
        return argument.substr(start);
    }

    StorageApi& sd_;
    StorageApi& internal_;

    Thread thread_ {};
    alignas(16) std::uint8_t stack_[0x8000] = {};
    bool threadStarted_ = false;
    std::atomic<bool> running_ {false};
    int listenFd_ = -1;
    std::atomic<int> clientFd_ {-1};
    std::atomic<int> clients_ {0};
    std::string address_;
    std::string advertisedIp_;

    mutable std::mutex logMutex_;
    std::vector<std::string> log_;

    int control_ = -1;
    int pasvFd_ = -1;
    std::string cwd_ = "/";
    std::string user_;
    std::string password_;
    std::string userGiven_;
    std::string peer_;
    std::string renameFrom_;
    bool authed_ = false;
};

std::unique_ptr<FtpController> createFtpServer(StorageApi& sd, StorageApi& internal) {
    return std::unique_ptr<FtpController>(new FtpServerImpl(sd, internal));
}

} // namespace nxf
