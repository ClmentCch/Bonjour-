/*
 * NXF Pro - NSP (PFS0) container inspection.
 *
 * An NSP is a PFS0 archive holding the ticket, the certificate, the CNMT and
 * the NCAs. Reading the table is the first half of an installation; writing
 * the contents into the console's NCM storage is the other half and needs the
 * CNMT (encrypted with the title key).
 *
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nxf {

struct NspContent {
    std::string name;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct NspInfo {
    bool valid = false;
    std::string error;
    std::uint32_t version = 0;
    std::uint64_t total = 0;
    int ncaCount = 0;
    bool hasTicket = false;
    bool hasCert = false;
    bool hasCnmt = false;
    bool hasXml = false;
    std::vector<NspContent> contents;
};

/// Reads the PFS0 table of `path`. Reads a few kilobytes at most, so it is
/// cheap enough to call for every .nsp found during a scan.
NspInfo parseNsp(const std::string& path);

} // namespace nxf
