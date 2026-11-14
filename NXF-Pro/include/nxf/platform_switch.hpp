/*
 * NXF Pro - Nintendo Switch platform services.
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <memory>
#include <string>

#include "nxf/app.hpp"

namespace nxf {

/// Backend selection: 0 = SD card, 1 = internal memory.
std::unique_ptr<StorageApi> openSdStorage();
std::unique_ptr<StorageApi> openInternalStorage();

/// Real FTP server bound to the console's network interface.
std::unique_ptr<FtpController> createFtpServer(StorageApi& sd, StorageApi& internal);

/// Installed applications, listed and removed through the console's ns service.
std::unique_ptr<TitleApi> createTitleApi();

/// Settings persistence (sdmc:/switch/NXF Pro/config.ini).
bool loadSettings(const std::string& path, SettingsState& out);
bool saveSettings(const std::string& path, const SettingsState& settings);

} // namespace nxf
