/*
 * NXF Pro - hekate boot configuration.
 *
 * hekate reads bootloader/hekate_ipl.ini at startup. Its [config] section
 * holds "autoboot", where 0 disables the automatic boot and 1..N selects the
 * entry to launch. NXF Pro edits that value without touching the boot entries.
 *
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <string>
#include <vector>

namespace nxf {

struct HekateConfig {
    bool exists = false;
    int autoboot = 0;
    std::vector<std::string> entries;
};

/// Parses hekate_ipl.ini: boot entry names plus the current autoboot value.
HekateConfig readHekateConfig(const std::string& path);

/// Writes "autoboot" inside [config], creating the file when needed.
/// autoboot = 0 disables the automatic boot.
bool writeHekateAutoboot(const std::string& path, int autoboot, std::string& error);

} // namespace nxf
