/*
 * NXF Pro - application state and interfaces.
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "nxf/canvas.hpp"
#include "nxf/font.hpp"
#include "nxf/icons.hpp"
#include "nxf/model.hpp"

namespace nxf {

enum class Section { Sd = 0, Internal = 1, Ftp = 2, Settings = 3, About = 4 };
enum class ViewMode { List, Grid };
enum class DialogKind { None, Confirm, Properties, Actions };

struct SettingsState {
    bool animations = true;
    bool gridDefault = false;
    int ftpPort = 5000;
    std::string ftpUser = "switch";
    std::string ftpPassword = "nxfpro";
};

struct InputState {
    float dt = 1.0f / 60.0f;
    float time = 0.0f;

    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool accept = false;
    bool back = false;
    bool select = false;
    bool actions = false;
    bool toggleView = false;
    bool cycleSort = false;
    bool search = false;
    bool prevSection = false;
    bool nextSection = false;
    bool quit = false;

    bool touchPressed = false;
    float touchX = 0.0f;
    float touchY = 0.0f;
};

/// Read/write access to one storage (SD card or internal memory).
struct StorageApi {
    virtual ~StorageApi() = default;
    virtual bool available() const = 0;
    virtual std::string displayName() const = 0;
    virtual std::string root() const = 0;
    virtual bool list(const std::string& path, std::vector<FileEntry>& out, std::string& error) = 0;
    virtual bool makeDirectory(const std::string& path, std::string& error) = 0;
    virtual bool createFile(const std::string& path, std::string& error) = 0;
    virtual bool renamePath(const std::string& from, const std::string& to, std::string& error) = 0;
    virtual bool removePath(const std::string& path, std::string& error) = 0;
    virtual bool copyPath(const std::string& from, const std::string& to, std::string& error) = 0;
    virtual FileStats stats(const std::string& path) = 0;
    virtual void space(std::uint64_t& total, std::uint64_t& free) = 0;
};

/// Host services the UI needs but cannot implement itself.
struct Platform {
    virtual ~Platform() = default;
    virtual bool promptText(const std::string& header, const std::string& initial,
                            std::string& out) = 0;
    virtual void settingsChanged(const SettingsState& settings) = 0;
};

struct FtpSnapshot {
    bool running = false;
    int clients = 0;
    std::string address;
    std::vector<std::string> log;
};

struct FtpController {
    virtual ~FtpController() = default;
    virtual bool start(int port, const std::string& user, const std::string& password,
                       std::string& error) = 0;
    virtual void stop() = 0;
    virtual FtpSnapshot snapshot() const = 0;
};

struct ExplorerState {
    std::string path;
    std::vector<FileEntry> entries;
    std::set<std::string> selection;
    int cursor = 0;
    float scroll = 0.0f;
    float scrollTarget = 0.0f;
    std::string filter;
    SortKey sort = SortKey::Name;
    bool ascending = true;
    ViewMode view = ViewMode::List;
    bool loaded = false;
    std::string error;
    float appear = 1.0f; // 0..1 staggered row reveal after a refresh
};

namespace layout {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;

constexpr float kSidebarWidth = 300.0f;
constexpr float kSidebarLogo = 84.0f;
constexpr float kSidebarLogoY = 28.0f;
constexpr float kSidebarItemX = 16.0f;
constexpr float kSidebarItemW = 268.0f;
constexpr float kSidebarItemH = 46.0f;
constexpr float kSidebarItemGap = 6.0f;

constexpr float kContentX = 348.0f;
constexpr float kContentRight = 1232.0f;
constexpr float kContentW = kContentRight - kContentX;

constexpr float kTitleY = 30.0f;
constexpr float kSubtitleY = 86.0f;
constexpr float kDividerY = 118.0f;
constexpr float kToolbarY = 134.0f;
constexpr float kListHeaderY = 188.0f;
constexpr float kContentTop = 214.0f;
constexpr float kContentBottom = 640.0f;
constexpr float kFooterY = 664.0f;

constexpr float kRowHeight = 44.0f;
constexpr float kRowGap = 5.0f;
constexpr float kGridCardW = 205.0f;
constexpr float kGridCardH = 132.0f;
constexpr float kGridGap = 12.0f;
constexpr int kGridColumns = 4;

constexpr float kVisibleListHeight = kContentBottom - kContentTop;

} // namespace layout

/// Sidebar geometry shared by input handling and rendering.
namespace nav {

constexpr int kCount = 5;

inline float itemY(int index) {
    switch (index) {
        case 0: return 166.0f;
        case 1: return 218.0f;
        case 2: return 304.0f;
        case 3: return 390.0f;
        case 4: return 442.0f;
        default: return 166.0f;
    }
}

inline int indexForSection(Section section) {
    switch (section) {
        case Section::Sd: return 0;
        case Section::Internal: return 1;
        case Section::Ftp: return 2;
        case Section::Settings: return 3;
        case Section::About: return 4;
    }
    return 0;
}

inline Section sectionFor(int index) {
    switch (index) {
        case 0: return Section::Sd;
        case 1: return Section::Internal;
        case 2: return Section::Ftp;
        case 3: return Section::Settings;
        case 4: return Section::About;
        default: return Section::Sd;
    }
}

inline int hitTest(float x, float y) {
    if (x < 0.0f || x > layout::kSidebarWidth) {
        return -1;
    }
    for (int i = 0; i < kCount; ++i) {
        const float top = itemY(i);
        if (y >= top && y <= top + layout::kSidebarItemH) {
            return i;
        }
    }
    return -1;
}

} // namespace nav

class App {
public:
    App(FontSet& fonts, Platform& platform, FtpController& ftp, StorageApi& sd,
        StorageApi& internal);

    void setLogo(const Image* logo) { logo_ = logo; }

    void update(const InputState& input);
    void render(Canvas& canvas);

    bool quitRequested() const { return quitRequested_; }
    bool settingsDirty() const { return settingsDirty_; }
    void clearSettingsDirty() { settingsDirty_ = false; }
    const SettingsState& settings() const { return settings_; }
    void applySettings(const SettingsState& settings) { settings_ = settings; }

    /// Applies the saved default listing mode to both storages.
    void setDefaultView(ViewMode mode) {
        sdView_.view = mode;
        internalView_.view = mode;
        settings_.gridDefault = mode == ViewMode::Grid;
    }

    /// Used by the platform to seed the initial directory.
    void setInitialPath(const std::string& path);

private:
    // State helpers
    ExplorerState& view();
    StorageApi& storage();
    StorageApi& storageFor(Section section);
    ExplorerState& viewFor(Section section);
    void switchSection(Section section);
    void refresh(ExplorerState& view, StorageApi& api);
    void openPath(ExplorerState& view, StorageApi& api, const std::string& path);
    void clampCursor(ExplorerState& view, StorageApi& api);
    void syncScroll(ExplorerState& view);
    void moveCursor(int delta);
    void activate();
    void goBack();
    void toggleSelect();
    void openActions();
    void openProperties();
    void requestDelete();
    void startRename();
    void startNewFolder();
    void startNewFile();
    void startSearch();
    void copySelection(bool cut);
    void pasteClipboard();
    std::vector<std::string> selectedNames(const ExplorerState& view) const;
    void runAction(int index);
    void showToast(const std::string& message);
    void closeDialog();
    void confirmDialog();
    void prompt(const std::string& header, const std::string& initial,
                const std::function<void(const std::string&)>& apply);

    // Input
    void updateDialog(const InputState& input);
    void updateExplorer(const InputState& input);
    void updateSectionNav(const InputState& input);
    void updateFtpScreen(const InputState& input);
    void updateSettingsScreen(const InputState& input);
    void updateTouch(const InputState& input);
    void handleShortcuts(const InputState& input);

    // Rendering (render.cpp)
    void renderSidebar(Canvas& canvas);
    void renderHeader(Canvas& canvas);
    void renderExplorer(Canvas& canvas, ExplorerState& view, StorageApi& api);
    void renderFtp(Canvas& canvas);
    void renderSettings(Canvas& canvas);
    void renderAbout(Canvas& canvas);
    void renderFooter(Canvas& canvas);
    void renderDialog(Canvas& canvas);
    void renderToast(Canvas& canvas);

    FontSet& fonts_;
    Platform& platform_;
    FtpController& ftp_;
    StorageApi& sd_;
    StorageApi& internal_;
    const Image* logo_ = nullptr;

    Section section_ = Section::Sd;
    Section previousSection_ = Section::Sd;
    float sectionProgress_ = 1.0f;
    float intro_ = 0.0f;
    float time_ = 0.0f;

    float navIndicatorY_ = 0.0f;
    bool navIndicatorReady_ = false;
    int hoveredNav_ = -1;

    ExplorerState sdView_;
    ExplorerState internalView_;
    SettingsState settings_;
    bool settingsDirty_ = false;
    bool quitRequested_ = false;

    DialogKind dialog_ = DialogKind::None;
    float dialogAnim_ = 0.0f;
    std::string dialogTitle_;
    std::string dialogMessage_;
    std::vector<std::string> dialogActions_;
    int dialogCursor_ = 0;
    int dialogFocus_ = 1; // 0 = cancel, 1 = confirm
    std::string dialogPath_;          // item the dialog acts on
    std::function<void(const std::string&)> dialogApply_;
    int pendingAction_ = -1;

    std::vector<std::string> clipboard_;
    bool clipboardCut_ = false;

    std::string toast_;
    float toastTimer_ = 0.0f;

    int settingsCursor_ = 0;
    int ftpFocus_ = 0;
    FtpSnapshot ftpState_;
    float pulse_ = 0.0f;

    float scrollVelocity_ = 0.0f;
};

} // namespace nxf
