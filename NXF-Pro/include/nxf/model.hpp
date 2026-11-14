/*
 * NXF Pro - file model, sorting and path helpers.
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nxf {

struct FileEntry {
    std::string name;
    bool isDirectory = false;
    std::uint64_t size = 0;
    std::int64_t modified = 0; // seconds since epoch, 0 when unknown
};

struct FileStats {
    bool exists = false;
    bool isDirectory = false;
    std::uint64_t size = 0;
    std::int64_t modified = 0;
};

enum class SortKey { Name = 0, Size = 1, Date = 2 };

/// Directories first, then the requested key.
void sortEntries(std::vector<FileEntry>& entries, SortKey key, bool ascending);

/// "1,4 Mo", "512 o", "3,2 Go" - French units, comma decimal separator.
std::string formatSize(std::uint64_t bytes);

/// "12/11/2026 20:04" (UTC), empty when the timestamp is unknown.
std::string formatDate(std::int64_t epochSeconds);

/// "Nom A-Z", "Taille Z-A"...
std::string sortLabel(SortKey key, bool ascending);

std::string joinPath(const std::string& base, const std::string& name);
std::string parentPath(const std::string& path);
std::string baseName(const std::string& path);
bool samePath(const std::string& a, const std::string& b);
bool isInsidePath(const std::string& child, const std::string& parent);

/// Splits "sdmc:/switch/foo" into {"switch", "foo"} for breadcrumbs.
std::vector<std::string> pathSegments(const std::string& path, const std::string& root);

bool matchesSearch(const FileEntry& entry, const std::string& query);

} // namespace nxf
