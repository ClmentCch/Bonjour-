/*
 * NXF Pro - file model helpers.
 * MIT licensed, see LICENSE.
 */
#include "nxf/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace nxf {

namespace {

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool lessNoCase(const std::string& a, const std::string& b) {
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const char ca = lower(a[i]);
        const char cb = lower(b[i]);
        if (ca != cb) {
            return ca < cb;
        }
    }
    return a.size() < b.size();
}

std::string trimmedNumber(double value, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    std::string text(buffer);
    // Trim trailing zeros, then a trailing dot.
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    std::replace(text.begin(), text.end(), '.', ',');
    return text;
}

// Days since 1970-01-01 -> civil date (Howard Hinnant's algorithm).
void civilFromDays(std::int64_t days, int& year, int& month, int& day) {
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t doe = days - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = yoe + era * 400;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
    const std::int64_t m = mp + (mp < 10 ? 3 : -9);
    year = static_cast<int>(y + (m <= 2 ? 1 : 0));
    month = static_cast<int>(m);
    day = static_cast<int>(d);
}

} // namespace

void sortEntries(std::vector<FileEntry>& entries, SortKey key, bool ascending) {
    std::stable_sort(entries.begin(), entries.end(), [key, ascending](const FileEntry& a,
                                                                      const FileEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory; // folders always first
        }
        bool less = false;
        switch (key) {
            case SortKey::Size:
                less = a.size != b.size ? a.size < b.size : lessNoCase(a.name, b.name);
                break;
            case SortKey::Date:
                less = a.modified != b.modified ? a.modified > b.modified
                                                : lessNoCase(a.name, b.name);
                break;
            case SortKey::Name:
            default:
                less = lessNoCase(a.name, b.name);
                break;
        }
        return ascending ? less : !less;
    });
}

std::string formatSize(std::uint64_t bytes) {
    if (bytes < 1024ull) {
        return std::to_string(bytes) + " o";
    }
    const double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        return trimmedNumber(kb, kb < 10.0 ? 1 : 0) + " Ko";
    }
    const double mb = kb / 1024.0;
    if (mb < 1024.0) {
        return trimmedNumber(mb, mb < 10.0 ? 1 : 0) + " Mo";
    }
    const double gb = mb / 1024.0;
    return trimmedNumber(gb, gb < 10.0 ? 2 : 1) + " Go";
}

std::string formatDate(std::int64_t epochSeconds) {
    if (epochSeconds <= 0) {
        return "-";
    }
    const std::int64_t days = epochSeconds / 86400;
    const std::int64_t secondsOfDay = epochSeconds % 86400;
    int year = 1970, month = 1, day = 1;
    civilFromDays(days, year, month, day);
    const int hour = static_cast<int>(secondsOfDay / 3600);
    const int minute = static_cast<int>((secondsOfDay % 3600) / 60);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d", day, month, year, hour, minute);
    return std::string(buffer);
}

std::string sortLabel(SortKey key, bool ascending) {
    const char* name = key == SortKey::Name ? "Nom" : (key == SortKey::Size ? "Taille" : "Date");
    return std::string(name) + (ascending ? " A-Z" : " Z-A");
}

std::string joinPath(const std::string& base, const std::string& name) {
    if (base.empty()) {
        return name;
    }
    if (base.back() == '/') {
        return base + name;
    }
    return base + "/" + name;
}

std::string parentPath(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    std::string trimmed = path;
    while (trimmed.size() > 1 && trimmed.back() == '/') {
        trimmed.pop_back();
    }
    const std::size_t slash = trimmed.find_last_of('/');
    if (slash == std::string::npos) {
        return trimmed;
    }
    // Keep the device root intact: "sdmc:/" -> "sdmc:/".
    const std::size_t colon = trimmed.find(':');
    if (colon != std::string::npos && slash <= colon + 1) {
        return trimmed.substr(0, colon + 2);
    }
    if (slash == 0) {
        return trimmed.substr(0, 1);
    }
    return trimmed.substr(0, slash);
}

std::string baseName(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    std::string trimmed = path;
    while (trimmed.size() > 1 && trimmed.back() == '/') {
        trimmed.pop_back();
    }
    const std::size_t slash = trimmed.find_last_of('/');
    if (slash == std::string::npos) {
        return trimmed;
    }
    if (slash + 1 >= trimmed.size()) {
        return trimmed;
    }
    return trimmed.substr(slash + 1);
}

bool samePath(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i])) {
            return false;
        }
    }
    return true;
}

bool isInsidePath(const std::string& child, const std::string& parent) {
    if (child.size() <= parent.size()) {
        return false;
    }
    for (std::size_t i = 0; i < parent.size(); ++i) {
        if (lower(child[i]) != lower(parent[i])) {
            return false;
        }
    }
    return child[parent.size()] == '/';
}

std::vector<std::string> pathSegments(const std::string& path, const std::string& root) {
    std::vector<std::string> segments;
    std::string relative = path;
    if (!root.empty() && path.size() >= root.size()) {
        bool prefix = true;
        for (std::size_t i = 0; i < root.size(); ++i) {
            if (lower(path[i]) != lower(root[i])) {
                prefix = false;
                break;
            }
        }
        if (prefix) {
            relative = path.substr(root.size());
        }
    }
    std::string current;
    for (char c : relative) {
        if (c == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

bool matchesSearch(const FileEntry& entry, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    if (query.size() > entry.name.size()) {
        return false;
    }
    for (std::size_t start = 0; start + query.size() <= entry.name.size(); ++start) {
        bool match = true;
        for (std::size_t i = 0; i < query.size(); ++i) {
            if (lower(entry.name[start + i]) != lower(query[i])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

} // namespace nxf
