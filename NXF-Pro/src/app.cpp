/*
 * NXF Pro - application logic.
 * MIT licensed, see LICENSE.
 */
#include "nxf/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace nxf {

namespace {

using namespace layout;

constexpr float kRowStride = kRowHeight + kRowGap;
constexpr float kGridStride = kGridCardH + kGridGap;

} // namespace

App::App(FontSet& fonts, Platform& platform, FtpController& ftp, StorageApi& sd,
         StorageApi& internal)
    : fonts_(fonts), platform_(platform), ftp_(ftp), sd_(sd), internal_(internal) {
    sdView_.path = sd_.root();
    internalView_.path = internal_.root();
    sdView_.view = settings_.gridDefault ? ViewMode::Grid : ViewMode::List;
    internalView_.view = sdView_.view;
}

void App::setInitialPath(const std::string& path) {
    if (!path.empty() && path.size() >= internal_.root().size() &&
        path.compare(0, internal_.root().size(), internal_.root()) == 0) {
        openPath(internalView_, internal_, path);
        section_ = Section::Internal;
    } else {
        openPath(sdView_, sd_, path.empty() ? sd_.root() : path);
        section_ = Section::Sd;
    }
    previousSection_ = section_;
}

ExplorerState& App::view() {
    return section_ == Section::Internal ? internalView_ : sdView_;
}

ExplorerState& App::viewFor(Section section) {
    return section == Section::Internal ? internalView_ : sdView_;
}

StorageApi& App::storage() {
    return section_ == Section::Internal ? internal_ : sd_;
}

StorageApi& App::storageFor(Section section) {
    return section == Section::Internal ? internal_ : sd_;
}

void App::switchSection(Section section) {
    if (section == section_) {
        return;
    }
    previousSection_ = section_;
    section_ = section;
    sectionProgress_ = 0.0f;
    dialog_ = DialogKind::None;
    ExplorerState& target = viewFor(section);
    if (!target.loaded) {
        refresh(target, storageFor(section));
    }
}

void App::refresh(ExplorerState& view, StorageApi& api) {
    if (!api.available()) {
        view.entries.clear();
        view.error = "Stockage indisponible";
        view.loaded = true;
        return;
    }
    std::vector<FileEntry> entries;
    std::string error;
    if (!api.list(view.path, entries, error)) {
        view.entries.clear();
        view.error = error.empty() ? "Impossible d'ouvrir ce dossier" : error;
        view.loaded = true;
        return;
    }
    view.error.clear();
    view.entries.clear();
    view.entries.reserve(entries.size());
    for (const FileEntry& entry : entries) {
        if (matchesSearch(entry, view.filter)) {
            view.entries.push_back(entry);
        }
    }
    sortEntries(view.entries, view.sort, view.ascending);
    view.loaded = true;
    view.appear = 0.0f;

    // Drop selections that no longer exist.
    for (auto it = view.selection.begin(); it != view.selection.end();) {
        const bool present = std::any_of(view.entries.begin(), view.entries.end(),
                                         [&](const FileEntry& e) { return e.name == *it; });
        it = present ? std::next(it) : view.selection.erase(it);
    }

    if (view.cursor >= static_cast<int>(view.entries.size())) {
        view.cursor = std::max(0, static_cast<int>(view.entries.size()) - 1);
    }
    view.scroll = 0.0f;
    view.scrollTarget = 0.0f;
    syncScroll(view);
    view.scroll = view.scrollTarget;
}

void App::openPath(ExplorerState& view, StorageApi& api, const std::string& path) {
    view.path = path;
    view.filter.clear();
    view.cursor = 0;
    view.selection.clear();
    refresh(view, api);
}

void App::clampCursor(ExplorerState& view, StorageApi& api) {
    (void)api;
    const int count = static_cast<int>(view.entries.size());
    if (count == 0) {
        view.cursor = 0;
        return;
    }
    view.cursor = std::clamp(view.cursor, 0, count - 1);
}

void App::syncScroll(ExplorerState& view) {
    const float visible = kVisibleListHeight;
    const int count = static_cast<int>(view.entries.size());
    if (count == 0) {
        view.scrollTarget = 0.0f;
        return;
    }

    if (view.view == ViewMode::List) {
        const float top = static_cast<float>(view.cursor) * kRowStride;
        const float bottom = top + kRowHeight;
        if (top < view.scrollTarget) {
            view.scrollTarget = top;
        } else if (bottom > view.scrollTarget + visible) {
            view.scrollTarget = bottom - visible;
        }
        const float maxScroll =
            std::max(0.0f, static_cast<float>(count) * kRowStride - kRowGap - visible);
        view.scrollTarget = std::clamp(view.scrollTarget, 0.0f, maxScroll);
    } else {
        const int rowIndex = view.cursor / kGridColumns;
        const float top = static_cast<float>(rowIndex) * kGridStride;
        const float bottom = top + kGridCardH;
        if (top < view.scrollTarget) {
            view.scrollTarget = top;
        } else if (bottom > view.scrollTarget + visible) {
            view.scrollTarget = bottom - visible;
        }
        const int rows = (count + kGridColumns - 1) / kGridColumns;
        const float maxScroll = std::max(0.0f, static_cast<float>(rows) * kGridStride - kGridGap -
                                                   visible);
        view.scrollTarget = std::clamp(view.scrollTarget, 0.0f, maxScroll);
    }
}

void App::moveCursor(int delta) {
    ExplorerState& state = view();
    const int count = static_cast<int>(state.entries.size());
    if (count == 0) {
        return;
    }
    if (state.view == ViewMode::Grid) {
        const int current = state.cursor;
        int next = current + delta;
        // Horizontal moves wrap to the next/previous row so the grid feels natural.
        if (delta == 1 && (current % kGridColumns) == kGridColumns - 1) {
            next = std::min(count - 1, current + 1);
        } else if (delta == -1 && (current % kGridColumns) == 0) {
            next = std::max(0, current - 1);
        }
        state.cursor = std::clamp(next, 0, count - 1);
    } else {
        state.cursor = std::clamp(state.cursor + delta, 0, count - 1);
    }
    syncScroll(state);
}

void App::activate() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    if (state.cursor < 0 || state.cursor >= static_cast<int>(state.entries.size())) {
        return;
    }
    const FileEntry& entry = state.entries[static_cast<std::size_t>(state.cursor)];
    if (entry.isDirectory) {
        openPath(state, api, joinPath(state.path, entry.name));
    } else {
        openProperties();
    }
}

void App::goBack() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    if (!state.filter.empty()) {
        state.filter.clear();
        refresh(state, api);
        showToast("Filtre effacé");
        return;
    }
    if (samePath(state.path, api.root())) {
        return;
    }
    const std::string parent = parentPath(state.path);
    const std::string previousName = baseName(state.path);
    openPath(state, api, parent);
    // Put the cursor back on the folder we came from.
    for (std::size_t i = 0; i < state.entries.size(); ++i) {
        if (state.entries[i].name == previousName) {
            state.cursor = static_cast<int>(i);
            syncScroll(state);
            break;
        }
    }
}

void App::toggleSelect() {
    ExplorerState& state = view();
    if (state.cursor < 0 || state.cursor >= static_cast<int>(state.entries.size())) {
        return;
    }
    const std::string& name = state.entries[static_cast<std::size_t>(state.cursor)].name;
    if (state.selection.count(name) > 0) {
        state.selection.erase(name);
    } else {
        state.selection.insert(name);
    }
}

std::vector<std::string> App::selectedNames(const ExplorerState& state) const {
    std::vector<std::string> names;
    if (!state.selection.empty()) {
        names.assign(state.selection.begin(), state.selection.end());
    } else if (state.cursor >= 0 && state.cursor < static_cast<int>(state.entries.size())) {
        names.push_back(state.entries[static_cast<std::size_t>(state.cursor)].name);
    }
    return names;
}

void App::openActions() {
    ExplorerState& state = view();
    const bool hasCursor = state.cursor >= 0 && state.cursor < static_cast<int>(state.entries.size());
    const bool isDir = hasCursor && state.entries[static_cast<std::size_t>(state.cursor)].isDirectory;
    const std::size_t selectionSize =
        state.selection.empty() ? (hasCursor ? 1u : 0u) : state.selection.size();

    dialogActions_.clear();
    if (isDir && selectionSize == 1) {
        dialogActions_.push_back("Ouvrir");
    }
    dialogActions_.push_back("Nouveau dossier");
    dialogActions_.push_back("Nouveau fichier texte");
    if (selectionSize == 1) {
        dialogActions_.push_back("Renommer");
    }
    if (selectionSize > 0) {
        dialogActions_.push_back("Copier");
        dialogActions_.push_back("Couper");
        dialogActions_.push_back("Supprimer");
        if (selectionSize == 1) {
            dialogActions_.push_back("Propriétés");
        }
    }
    if (!clipboard_.empty()) {
        dialogActions_.push_back("Coller");
    }

    dialog_ = DialogKind::Actions;
    dialogAnim_ = 0.0f;
    dialogCursor_ = 0;
    dialogPath_ = state.path;
}

void App::openProperties() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    if (state.cursor < 0 || state.cursor >= static_cast<int>(state.entries.size())) {
        return;
    }
    const FileEntry& entry = state.entries[static_cast<std::size_t>(state.cursor)];
    const std::string full = joinPath(state.path, entry.name);

    FileStats stats = api.stats(full);
    const std::uint64_t size = stats.exists ? stats.size : entry.size;
    const std::int64_t modified = stats.exists ? stats.modified : entry.modified;

    dialogActions_.clear();
    dialogActions_.push_back(entry.name);
    dialogActions_.push_back(entry.isDirectory ? "Dossier" : "Fichier");
    dialogActions_.push_back(entry.isDirectory ? "—" : formatSize(size));
    dialogActions_.push_back(formatDate(modified));
    dialogActions_.push_back(full);

    dialog_ = DialogKind::Properties;
    dialogAnim_ = 0.0f;
}

void App::requestDelete() {
    ExplorerState& state = view();
    const std::vector<std::string> names = selectedNames(state);
    if (names.empty()) {
        return;
    }
    dialogTitle_ = "Supprimer";
    if (names.size() == 1) {
        dialogMessage_ = "« " + names[0] + " » sera supprimé définitivement. Cette action est irréversible.";
    } else {
        dialogMessage_ = std::to_string(names.size()) +
                         " éléments seront supprimés définitivement. Cette action est irréversible.";
    }
    dialogActions_ = names;
    dialog_ = DialogKind::Confirm;
    dialogAnim_ = 0.0f;
    dialogFocus_ = 0;
    pendingAction_ = 1;
}

void App::closeDialog() {
    dialog_ = DialogKind::None;
    dialogAnim_ = 0.0f;
    pendingAction_ = -1;
    dialogActions_.clear();
}

void App::prompt(const std::string& header, const std::string& initial,
                 const std::function<void(const std::string&)>& apply) {
    std::string value;
    if (!platform_.promptText(header, initial, value)) {
        return;
    }
    if (value.empty()) {
        return;
    }
    apply(value);
}

void App::startNewFolder() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    const std::string path = state.path;
    prompt("Nom du dossier", "", [this, &state, &api, path](const std::string& name) {
        std::string error;
        if (api.makeDirectory(joinPath(path, name), error)) {
            refresh(state, api);
            showToast("Dossier créé");
        } else {
            showToast(error.empty() ? "Création impossible" : error);
        }
    });
}

void App::startNewFile() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    const std::string path = state.path;
    prompt("Nom du fichier", "", [this, &state, &api, path](const std::string& name) {
        std::string finalName = name;
        if (finalName.find('.') == std::string::npos) {
            finalName += ".txt";
        }
        std::string error;
        if (api.createFile(joinPath(path, finalName), error)) {
            refresh(state, api);
            showToast("Fichier créé");
        } else {
            showToast(error.empty() ? "Création impossible" : error);
        }
    });
}

void App::startRename() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    if (state.cursor < 0 || state.cursor >= static_cast<int>(state.entries.size())) {
        return;
    }
    const std::string path = state.path;
    const std::string oldName = state.entries[static_cast<std::size_t>(state.cursor)].name;

    prompt("Renommer", oldName, [this, &state, &api, path, oldName](const std::string& name) {
        if (name == oldName) {
            return;
        }
        std::string error;
        if (api.renamePath(joinPath(path, oldName), joinPath(path, name), error)) {
            refresh(state, api);
            showToast("Renommé");
        } else {
            showToast(error.empty() ? "Renommage impossible" : error);
        }
    });
}

void App::startSearch() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    prompt("Rechercher dans ce dossier", state.filter, [this, &state, &api](const std::string& query) {
        state.filter = query;
        state.cursor = 0;
        refresh(state, api);
        if (!query.empty()) {
            showToast(std::to_string(state.entries.size()) + " résultat(s)");
        }
    });
}

void App::copySelection(bool cut) {
    ExplorerState& state = view();
    clipboard_.clear();
    for (const std::string& name : selectedNames(state)) {
        clipboard_.push_back(joinPath(state.path, name));
    }
    clipboardCut_ = cut;
    if (!clipboard_.empty()) {
        showToast(cut ? "Éléments coupés" : "Éléments copiés");
    }
}

void App::pasteClipboard() {
    ExplorerState& state = view();
    StorageApi& api = storage();
    if (clipboard_.empty()) {
        return;
    }
    const std::string destination = state.path;
    int done = 0;
    std::string lastError;

    for (const std::string& source : clipboard_) {
        std::string name = baseName(source);
        std::string target = joinPath(destination, name);

        // Never overwrite: pick a free name.
        int suffix = 0;
        while (api.stats(target).exists) {
            ++suffix;
            const std::size_t dot = name.find_last_of('.');
            const std::string base = dot == std::string::npos ? name : name.substr(0, dot);
            const std::string extension = dot == std::string::npos ? "" : name.substr(dot);
            target = joinPath(destination, base + " (" + std::to_string(suffix) + ")" + extension);
            if (suffix > 200) {
                break;
            }
        }

        std::string error;
        const bool ok = clipboardCut_ ? api.renamePath(source, target, error)
                                      : api.copyPath(source, target, error);
        if (ok) {
            ++done;
        } else if (lastError.empty()) {
            lastError = error;
        }
    }

    if (done > 0) {
        clipboard_.clear();
        clipboardCut_ = false;
    }
    refresh(state, api);
    if (done > 0) {
        showToast(std::to_string(done) + " élément(s) collé(s)");
    } else {
        showToast(lastError.empty() ? "Collage impossible" : lastError);
    }
}

void App::runAction(int index) {
    if (index < 0 || index >= static_cast<int>(dialogActions_.size())) {
        return;
    }
    const std::string action = dialogActions_[static_cast<std::size_t>(index)];
    closeDialog();

    if (action == "Ouvrir") {
        activate();
    } else if (action == "Nouveau dossier") {
        startNewFolder();
    } else if (action == "Nouveau fichier texte") {
        startNewFile();
    } else if (action == "Renommer") {
        startRename();
    } else if (action == "Copier") {
        copySelection(false);
    } else if (action == "Couper") {
        copySelection(true);
    } else if (action == "Coller") {
        pasteClipboard();
    } else if (action == "Supprimer") {
        requestDelete();
    } else if (action == "Propriétés") {
        openProperties();
    }
}

void App::confirmDialog() {
    if (pendingAction_ != 1) {
        closeDialog();
        return;
    }
    ExplorerState& state = view();
    StorageApi& api = storage();
    const std::vector<std::string> names = dialogActions_;
    closeDialog();

    int removed = 0;
    std::string lastError;
    for (const std::string& name : names) {
        std::string error;
        if (api.removePath(joinPath(state.path, name), error)) {
            ++removed;
        } else if (lastError.empty()) {
            lastError = error;
        }
    }
    state.selection.clear();
    refresh(state, api);
    if (removed > 0) {
        showToast(std::to_string(removed) + " élément(s) supprimé(s)");
    } else {
        showToast(lastError.empty() ? "Suppression impossible" : lastError);
    }
}

void App::showToast(const std::string& message) {
    toast_ = message;
    toastTimer_ = 2.6f;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void App::update(const InputState& input) {
    time_ = input.time;
    const float dt = std::min(input.dt, 0.1f);
    const float animScale = settings_.animations ? 1.0f : 20.0f;

    intro_ = std::min(1.0f, intro_ + dt * animScale / 0.45f);
    sectionProgress_ = std::min(1.0f, sectionProgress_ + dt * animScale / 0.22f);
    pulse_ += dt;

    if (toastTimer_ > 0.0f) {
        toastTimer_ -= dt;
        if (toastTimer_ <= 0.0f) {
            toast_.clear();
        }
    }

    if (dialog_ != DialogKind::None) {
        dialogAnim_ = std::min(1.0f, dialogAnim_ + dt * animScale / 0.16f);
    } else {
        dialogAnim_ = std::max(0.0f, dialogAnim_ - dt * animScale / 0.12f);
    }

    for (ExplorerState* state : {&sdView_, &internalView_}) {
        state->appear = std::min(1.0f, state->appear + dt * animScale / 0.55f);
        state->scroll = approach(state->scroll, state->scrollTarget, 16.0f * animScale, dt);
    }
    navIndicatorY_ = approach(navIndicatorY_, nav::itemY(nav::indexForSection(section_)) + 12.0f,
                              16.0f * animScale, dt);

    ftpState_ = ftp_.snapshot();

    if (input.quit) {
        quitRequested_ = true;
        return;
    }

    if (dialog_ != DialogKind::None) {
        updateDialog(input);
        return;
    }

    handleShortcuts(input);
    if (quitRequested_) {
        return;
    }

    switch (section_) {
        case Section::Sd:
        case Section::Internal:
            updateExplorer(input);
            break;
        case Section::Ftp:
            updateFtpScreen(input);
            break;
        case Section::Settings:
            updateSettingsScreen(input);
            break;
        case Section::About:
            break;
    }

    updateTouch(input);
}

void App::handleShortcuts(const InputState& input) {
    if (input.prevSection) {
        const int index = std::max(0, static_cast<int>(section_) - 1);
        switchSection(static_cast<Section>(index));
    }
    if (input.nextSection) {
        const int index = std::min(4, static_cast<int>(section_) + 1);
        switchSection(static_cast<Section>(index));
    }
    if (input.toggleView && (section_ == Section::Sd || section_ == Section::Internal)) {
        ExplorerState& state = view();
        state.view = state.view == ViewMode::List ? ViewMode::Grid : ViewMode::List;
        syncScroll(state);
        showToast(state.view == ViewMode::List ? "Vue liste" : "Vue grille");
    }
    if (input.cycleSort && (section_ == Section::Sd || section_ == Section::Internal)) {
        ExplorerState& state = view();
        if (state.sort == SortKey::Name) {
            state.sort = SortKey::Size;
            state.ascending = true;
        } else if (state.sort == SortKey::Size) {
            state.sort = SortKey::Date;
            state.ascending = false;
        } else {
            state.sort = SortKey::Name;
            state.ascending = !state.ascending;
        }
        refresh(state, storage());
        showToast("Tri : " + sortLabel(state.sort, state.ascending));
    }
    if (input.search && (section_ == Section::Sd || section_ == Section::Internal)) {
        startSearch();
    }
}

void App::updateExplorer(const InputState& input) {
    ExplorerState& state = view();
    StorageApi& api = storage();

    if (!state.loaded) {
        refresh(state, api);
    }

    if (state.view == ViewMode::Grid) {
        if (input.up) {
            moveCursor(-kGridColumns);
        }
        if (input.down) {
            moveCursor(kGridColumns);
        }
        if (input.left) {
            moveCursor(-1);
        }
        if (input.right) {
            moveCursor(1);
        }
    } else {
        if (input.up) {
            moveCursor(-1);
        }
        if (input.down) {
            moveCursor(1);
        }
        if (input.left) {
            moveCursor(-5);
        }
        if (input.right) {
            moveCursor(5);
        }
    }

    if (input.accept) {
        activate();
    }
    if (input.back) {
        goBack();
    }
    if (input.select) {
        toggleSelect();
    }
    if (input.actions) {
        openActions();
    }
    if (input.search) {
        // handled by handleShortcuts for sections; keep here for safety
    }

    clampCursor(state, api);
    syncScroll(state);
}

void App::updateFtpScreen(const InputState& input) {
    if (input.accept) {
        if (ftpState_.running) {
            ftp_.stop();
            showToast("Serveur FTP arrêté");
        } else {
            std::string error;
            if (ftp_.start(settings_.ftpPort, settings_.ftpUser, settings_.ftpPassword, error)) {
                showToast("Serveur FTP démarré");
            } else {
                showToast(error.empty() ? "Démarrage impossible" : error);
            }
        }
        ftpState_ = ftp_.snapshot();
    }
}

void App::updateSettingsScreen(const InputState& input) {
    constexpr int kRowCount = 5;
    if (input.up) {
        settingsCursor_ = (settingsCursor_ + kRowCount - 1) % kRowCount;
    }
    if (input.down) {
        settingsCursor_ = (settingsCursor_ + 1) % kRowCount;
    }
    if (!input.accept) {
        return;
    }

    switch (settingsCursor_) {
        case 0:
            settings_.animations = !settings_.animations;
            settingsDirty_ = true;
            showToast(settings_.animations ? "Animations activées" : "Animations désactivées");
            break;
        case 1:
            settings_.gridDefault = !settings_.gridDefault;
            sdView_.view = settings_.gridDefault ? ViewMode::Grid : ViewMode::List;
            internalView_.view = sdView_.view;
            settingsDirty_ = true;
            showToast(settings_.gridDefault ? "Vue grille par défaut" : "Vue liste par défaut");
            break;
        case 2: {
            const std::string current = std::to_string(settings_.ftpPort);
            prompt("Port du serveur FTP", current, [this](const std::string& value) {
                const int port = std::atoi(value.c_str());
                if (port > 1024 && port < 65536) {
                    settings_.ftpPort = port;
                    settingsDirty_ = true;
                    showToast("Port " + std::to_string(port));
                } else {
                    showToast("Port invalide (1025-65535)");
                }
            });
            break;
        }
        case 3:
            prompt("Utilisateur FTP", settings_.ftpUser, [this](const std::string& value) {
                settings_.ftpUser = value;
                settingsDirty_ = true;
                showToast("Utilisateur enregistré");
            });
            break;
        case 4:
            prompt("Mot de passe FTP", settings_.ftpPassword, [this](const std::string& value) {
                settings_.ftpPassword = value;
                settingsDirty_ = true;
                showToast("Mot de passe enregistré");
            });
            break;
        default:
            break;
    }

    if (settingsDirty_) {
        platform_.settingsChanged(settings_);
    }
}

void App::updateDialog(const InputState& input) {
    if (dialog_ == DialogKind::Confirm) {
        if (input.left || input.right) {
            dialogFocus_ = dialogFocus_ == 0 ? 1 : 0;
        }
        if (input.accept) {
            if (dialogFocus_ == 1) {
                confirmDialog();
            } else {
                closeDialog();
            }
        }
        if (input.back) {
            closeDialog();
        }
        return;
    }

    if (dialog_ == DialogKind::Actions) {
        const int count = static_cast<int>(dialogActions_.size());
        if (input.up && count > 0) {
            dialogCursor_ = (dialogCursor_ + count - 1) % count;
        }
        if (input.down && count > 0) {
            dialogCursor_ = (dialogCursor_ + 1) % count;
        }
        if (input.accept) {
            runAction(dialogCursor_);
        }
        if (input.back) {
            closeDialog();
        }
        return;
    }

    if (input.back || input.accept) {
        closeDialog();
    }
}

void App::updateTouch(const InputState& input) {
    if (!input.touchPressed) {
        hoveredNav_ = -1;
        return;
    }

    const int nav = nav::hitTest(input.touchX, input.touchY);
    hoveredNav_ = nav;
    if (nav >= 0) {
        switchSection(nav::sectionFor(nav));
        return;
    }

    if (section_ == Section::Ftp) {
        if (input.touchX > kContentRight - 220.0f && input.touchX < kContentRight &&
            input.touchY > kContentTop + 40.0f && input.touchY < kContentTop + 100.0f) {
            if (ftpState_.running) {
                ftp_.stop();
                showToast("Serveur FTP arrêté");
            } else {
                std::string error;
                if (ftp_.start(settings_.ftpPort, settings_.ftpUser, settings_.ftpPassword, error)) {
                    showToast("Serveur FTP démarré");
                } else {
                    showToast(error.empty() ? "Démarrage impossible" : error);
                }
            }
            ftpState_ = ftp_.snapshot();
        }
        return;
    }

    if (section_ != Section::Sd && section_ != Section::Internal) {
        return;
    }

    ExplorerState& state = view();
    StorageApi& api = storage();
    if (input.touchX < kContentX || input.touchY < kContentTop || input.touchY > kContentBottom) {
        return;
    }

    const float localY = input.touchY - kContentTop + state.scroll;
    int index = -1;
    if (state.view == ViewMode::List) {
        index = static_cast<int>(localY / kRowStride);
    } else {
        const float localX = input.touchX - kContentX;
        const int column = static_cast<int>(localX / (kGridCardW + kGridGap));
        const int rowIndex = static_cast<int>(localY / kGridStride);
        if (column >= 0 && column < kGridColumns) {
            index = rowIndex * kGridColumns + column;
        }
    }
    if (index < 0 || index >= static_cast<int>(state.entries.size())) {
        return;
    }

    if (index == state.cursor) {
        activate();
    } else {
        state.cursor = index;
        syncScroll(state);
    }
    (void)api;
}

} // namespace nxf
