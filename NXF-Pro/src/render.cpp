/*
 * NXF Pro - interface rendering.
 * MIT licensed, see LICENSE.
 */
#include "nxf/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace nxf {

namespace {

using namespace layout;

const FontSet* gFonts = nullptr;

struct TextParams {
    Weight weight = Weight::Regular;
    float size = 15.0f;
    Color color = theme::kText;
    float opacity = 1.0f;
    float spacing = 0.0f;
};

float measureText(const std::string& text, const TextParams& params) {
    TextStyle style;
    style.weight = params.weight;
    style.size = params.size;
    style.letterSpacing = params.spacing;
    return gFonts->measure(style, text);
}

void drawText(Canvas& canvas, float x, float y, const std::string& text, const TextParams& params) {
    TextStyle style;
    style.weight = params.weight;
    style.size = params.size;
    style.color = params.color;
    style.opacity = params.opacity;
    style.letterSpacing = params.spacing;
    gFonts->draw(canvas, style, x, y, text);
}

/// `centerY` is the visual middle of the line.
void drawTextVCentered(Canvas& canvas, float x, float centerY, const std::string& text,
                       const TextParams& params) {
    const float ascent = gFonts->ascent(params.weight, params.size);
    const float top = centerY - ascent + params.size * 0.36f;
    drawText(canvas, x, top, text, params);
}

void drawTextRight(Canvas& canvas, float right, float centerY, const std::string& text,
                   const TextParams& params) {
    const float width = measureText(text, params);
    drawTextVCentered(canvas, right - width, centerY, text, params);
}

void drawTextCentered(Canvas& canvas, float centerX, float centerY, const std::string& text,
                      const TextParams& params) {
    const float width = measureText(text, params);
    drawTextVCentered(canvas, centerX - width * 0.5f, centerY, text, params);
}

std::vector<std::string> wrapText(const std::string& text, const TextParams& params, float maxWidth) {
    std::vector<std::string> lines;
    std::string current;
    std::string word;
    auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (measureText(candidate, params) <= maxWidth) {
            current = candidate;
        } else {
            if (!current.empty()) {
                lines.push_back(current);
            }
            current = word;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') {
            flushWord();
        } else {
            word.push_back(c);
        }
    }
    flushWord();
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

void drawCard(Canvas& canvas, const Rect& rect, float radius, Color fill) {
    canvas.fillRoundRect(rect, radius, fill);
    canvas.strokeRoundRect(rect, radius, theme::kBorder, 1.0f);
}

Icon sectionIcon(Section section) {
    switch (section) {
        case Section::Sd: return Icon::SdCard;
        case Section::Internal: return Icon::Memory;
        case Section::Ftp: return Icon::Network;
        case Section::Apps: return Icon::Apps;
        case Section::Settings: return Icon::Settings;
        case Section::About: return Icon::Info;
    }
    return Icon::Info;
}

const char* sectionLabel(Section section) {
    switch (section) {
        case Section::Sd: return "Carte SD";
        case Section::Internal: return "Mémoire interne";
        case Section::Apps: return "Applications";
        case Section::Ftp: return "Serveur FTP";
        case Section::Settings: return "Paramètres";
        case Section::About: return "À propos";
    }
    return "";
}

struct NavEntry {
    const char* label;
    Icon icon;
    Section section;
};

const NavEntry kNav[] = {
    {"Switch SD", Icon::SdCard, Section::Sd},
    {"Mémoire interne", Icon::Memory, Section::Internal},
    {"Applications", Icon::Apps, Section::Apps},
    {"FTP", Icon::Network, Section::Ftp},
    {"Paramètres", Icon::Settings, Section::Settings},
    {"À propos", Icon::Info, Section::About},
};

void drawPill(Canvas& canvas, const Rect& rect, const std::string& label, Color dot,
              Color textColor) {
    canvas.fillRoundRect(rect, rect.h * 0.5f, theme::kPanelHover);
    canvas.strokeRoundRect(rect, rect.h * 0.5f, theme::kBorder, 1.0f);
    canvas.fillCircle(rect.x + 14.0f, rect.y + rect.h * 0.5f, 3.5f, dot);
    TextParams params;
    params.weight = Weight::Medium;
    params.size = 12.0f;
    params.color = textColor;
    drawTextVCentered(canvas, rect.x + 24.0f, rect.y + rect.h * 0.5f, label, params);
}

void drawChip(Canvas& canvas, const Rect& rect, const std::string& label, Icon icon,
              bool active, float opacity = 1.0f) {
    const Color fill = active ? theme::kPanelActive : theme::kPanel;
    canvas.fillRoundRect(rect, 10.0f, withAlpha(fill, opacity));
    canvas.strokeRoundRect(rect, 10.0f, withAlpha(theme::kBorder, opacity), 1.0f);

    TextParams params;
    params.weight = Weight::Regular;
    params.size = 13.0f;
    params.color = active ? theme::kText : theme::kTextSecondary;
    params.opacity = opacity;

    if (icon != Icon::Close) {
        const float iconSize = 15.0f;
        drawIcon(canvas, icon, Rect{rect.x + 12.0f, rect.y + (rect.h - iconSize) * 0.5f, iconSize,
                                    iconSize},
                 params.color, opacity * 0.85f);
        drawTextVCentered(canvas, rect.x + 12.0f + iconSize + 8.0f, rect.y + rect.h * 0.5f, label,
                          params);
    } else {
        drawTextVCentered(canvas, rect.x + 12.0f, rect.y + rect.h * 0.5f, label, params);
    }
}

void drawToggle(Canvas& canvas, const Rect& rect, bool on) {
    canvas.fillRoundRect(rect, rect.h * 0.5f, on ? theme::kAccent : theme::kPanelActive);
    const float knob = rect.h - 8.0f;
    const float x = on ? rect.right() - knob - 4.0f : rect.x + 4.0f;
    canvas.fillCircle(x + knob * 0.5f, rect.y + rect.h * 0.5f, knob * 0.5f, theme::kText);
}

void drawButton(Canvas& canvas, const Rect& rect, const std::string& label, bool primary,
                bool focused, float opacity, bool danger = false) {
    if (danger) {
        canvas.fillRoundRect(rect, 12.0f, withAlpha(theme::kDanger, opacity));
    } else if (primary) {
        canvas.fillRoundRect(rect, 12.0f, withAlpha(theme::kAccent, opacity));
    } else {
        canvas.fillRoundRect(rect, 12.0f, withAlpha(theme::kPanelHover, opacity));
        canvas.strokeRoundRect(rect, 12.0f, withAlpha(theme::kBorder, opacity), 1.0f);
    }
    if (focused) {
        canvas.strokeRoundRect(Rect{rect.x - 3.0f, rect.y - 3.0f, rect.w + 6.0f, rect.h + 6.0f},
                               15.0f, withAlpha(theme::kAccent, 0.75f * opacity), 1.5f);
    }
    TextParams params;
    params.weight = Weight::Medium;
    params.size = 14.0f;
    params.color = (primary || danger) ? hex(0x0A0A0C) : theme::kText;
    params.opacity = opacity;
    drawTextCentered(canvas, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, label, params);
}

} // namespace

// ---------------------------------------------------------------------------
// App::render
// ---------------------------------------------------------------------------
void App::render(Canvas& canvas) {
    gFonts = &fonts_;

    canvas.clear(theme::kBackground);

    const float introFade = easeOutCubic(intro_);
    renderSidebar(canvas);
    renderHeader(canvas);

    canvas.pushClip(Rect{kContentX - 8.0f, kToolbarY - 6.0f, kContentW + 16.0f,
                         kContentBottom - kToolbarY + 12.0f});
    switch (section_) {
        case Section::Sd:
            renderExplorer(canvas, sdView_, sd_);
            break;
        case Section::Internal:
            renderExplorer(canvas, internalView_, internal_);
            break;
        case Section::Apps:
            renderApps(canvas);
            break;
        case Section::Ftp:
            renderFtp(canvas);
            break;
        case Section::Settings:
            renderSettings(canvas);
            break;
        case Section::About:
            renderAbout(canvas);
            break;
    }
    canvas.popClip();

    // Content cross-fade between sections.
    if (sectionProgress_ < 1.0f) {
        canvas.fillRect(Rect{kContentX - 8.0f, kToolbarY - 6.0f, kContentW + 16.0f,
                             kContentBottom - kToolbarY + 12.0f},
                        withAlpha(theme::kBackground, 1.0f - easeOutCubic(sectionProgress_)));
    }

    renderFooter(canvas);
    renderDialog(canvas);
    renderToast(canvas);

    if (introFade < 1.0f) {
        canvas.dim(1.0f - introFade);
    }
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------
void App::renderSidebar(Canvas& canvas) {
    canvas.fillRect(Rect{0, 0, kSidebarWidth, static_cast<float>(kScreenHeight)},
                    theme::kSidebar);
    canvas.fillRect(Rect{kSidebarWidth - 1.0f, 0, 1, static_cast<float>(kScreenHeight)},
                    theme::kSidebarEdge);

    // Logo
    const float logoX = (kSidebarWidth - kSidebarLogo) * 0.5f;
    if (logo_ != nullptr && logo_->valid()) {
        canvas.drawImageRounded(*logo_, Rect{logoX, kSidebarLogoY, kSidebarLogo, kSidebarLogo},
                                20.0f, 1.0f);
    } else {
        // Neutral typographic fallback until assets/logonxf.png exists.
        std::vector<Color> columns;
        TextParams params;
        params.weight = Weight::Medium;
        params.size = 26.0f;
        const std::string word = "NXF";
        const float width = measureText(word, params);
        columns.resize(static_cast<std::size_t>(width) + 4);
        for (std::size_t i = 0; i < columns.size(); ++i) {
            columns[i] = rainbow(static_cast<float>(i) / static_cast<float>(columns.size()) +
                                 time_ * 0.15f);
        }
        TextStyle style;
        style.weight = Weight::Medium;
        style.size = 26.0f;
        const float x = (kSidebarWidth - width) * 0.5f;
        gFonts->drawGradient(canvas, style, x, kSidebarLogoY + 18.0f, word, columns,
                             static_cast<int>(x));
        TextParams sub;
        sub.weight = Weight::Light;
        sub.size = 11.0f;
        sub.color = theme::kTextMuted;
        sub.spacing = 3.0f;
        drawTextCentered(canvas, kSidebarWidth * 0.5f, kSidebarLogoY + 66.0f, "PRO", sub);
    }

    // Group labels
    TextParams group;
    group.weight = Weight::Medium;
    group.size = 10.0f;
    group.color = theme::kTextMuted;
    group.spacing = 1.6f;
    drawText(canvas, 32.0f, 142.0f, "STOCKAGE", group);
    drawText(canvas, 32.0f, 280.0f, "CONSOLE", group);
    drawText(canvas, 32.0f, 366.0f, "RÉSEAU", group);
    drawText(canvas, 32.0f, 452.0f, "APPLICATION", group);

    // Sliding active indicator
    const float targetY = nav::itemY(nav::indexForSection(section_)) + 12.0f;
    if (!navIndicatorReady_) {
        navIndicatorY_ = targetY;
        navIndicatorReady_ = true;
    }

    for (int i = 0; i < nav::kCount; ++i) {
        const NavEntry& entry = kNav[i];
        const float y = nav::itemY(i);
        const bool active = section_ == entry.section;
        const bool hovered = hoveredNav_ == i;
        const Rect item{kSidebarItemX, y, kSidebarItemW, kSidebarItemH};

        if (active || hovered) {
            const Color fill = active ? theme::kPanelActive : theme::kPanelHover;
            canvas.fillRoundRect(item, 12.0f, withAlpha(fill, active ? 1.0f : 0.75f));
            if (active) {
                canvas.strokeRoundRect(item, 12.0f, theme::kBorderStrong, 1.0f);
            }
        }

        const Color iconColor = active ? theme::kText : theme::kTextSecondary;
        drawIcon(canvas, entry.icon,
                 Rect{item.x + 16.0f, y + (kSidebarItemH - 19.0f) * 0.5f, 19.0f, 19.0f},
                 iconColor, active ? 1.0f : 0.8f);

        TextParams params;
        params.weight = active ? Weight::Medium : Weight::Regular;
        params.size = 14.0f;
        params.color = active ? theme::kText : theme::kTextSecondary;
        drawTextVCentered(canvas, item.x + 46.0f, y + kSidebarItemH * 0.5f, entry.label, params);

        if (active) {
            canvas.fillCircle(item.right() - 18.0f, y + kSidebarItemH * 0.5f, 2.5f,
                              theme::kTextSecondary);
        }
    }

    // Rainbow bar next to the active entry (drawn after so it stays on top).
    const Rect bar{kSidebarItemX + 1.0f, navIndicatorY_, 3.0f, 22.0f};
    canvas.fillRoundRect(bar, 1.5f, rainbow(time_ * 0.25f));
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------
void App::renderHeader(Canvas& canvas) {
    const std::string title = "NXF Pro";

    TextParams params;
    params.weight = Weight::Light;
    params.size = 40.0f;
    const float width = measureText(title, params);
    const int columnsCount = static_cast<int>(std::ceil(width)) + 6;
    std::vector<Color> columns(static_cast<std::size_t>(columnsCount));
    for (int i = 0; i < columnsCount; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(columnsCount);
        columns[static_cast<std::size_t>(i)] = rainbow(u * 1.15f - time_ * 0.22f);
    }

    TextStyle style;
    style.weight = Weight::Light;
    style.size = 40.0f;
    gFonts->drawGradient(canvas, style, kContentX, kTitleY + 8.0f, title, columns,
                         static_cast<int>(kContentX));

    TextParams subtitle;
    subtitle.weight = Weight::Medium;
    subtitle.size = 11.0f;
    subtitle.color = theme::kTextMuted;
    subtitle.spacing = 1.8f;
    drawText(canvas, kContentX + 2.0f, kSubtitleY, sectionLabel(section_), subtitle);

    // Storage usage on the right of the header.
    StorageApi& api = storage();
    if (section_ == Section::Sd || section_ == Section::Internal) {
        std::uint64_t total = 0;
        std::uint64_t free = 0;
        api.space(total, free);
        if (total > 0) {
            char buffer[96];
            const std::string used = formatSize(total - free);
            const std::string capacity = formatSize(total);
            std::snprintf(buffer, sizeof(buffer), "%s libres sur %s", formatSize(free).c_str(),
                          capacity.c_str());
            TextParams usage;
            usage.weight = Weight::Light;
            usage.size = 12.0f;
            usage.color = theme::kTextMuted;
            drawTextRight(canvas, kContentRight, kSubtitleY + 7.0f, buffer, usage);
        }
    }

    canvas.fillRect(Rect{kContentX, kDividerY, kContentW, 1.0f}, theme::kSidebarEdge);
}

// ---------------------------------------------------------------------------
// Explorer
// ---------------------------------------------------------------------------
void App::renderExplorer(Canvas& canvas, ExplorerState& view, StorageApi& api) {
    if (!api.available()) {
        const Rect panel{kContentX, kContentTop + 40.0f, kContentW, 200.0f};
        drawCard(canvas, panel, 16.0f, theme::kPanel);
        drawIcon(canvas, Icon::Warning, Rect{panel.x + 28.0f, panel.y + 30.0f, 26.0f, 26.0f},
                 theme::kWarning);
        TextParams title;
        title.weight = Weight::Medium;
        title.size = 17.0f;
        drawText(canvas, panel.x + 68.0f, panel.y + 32.0f, "Mémoire interne indisponible", title);

        TextParams body;
        body.weight = Weight::Light;
        body.size = 13.0f;
        body.color = theme::kTextSecondary;
        const std::vector<std::string> lines = wrapText(
            "Cette partition n'est pas accessible dans le mode de lancement actuel "
            "(applet). Lancez NXF Pro depuis le menu HOME en maintenant R sur un jeu "
            "pour obtenir les permissions d'application.",
            body, panel.w - 96.0f);
        float y = panel.y + 66.0f;
        for (const std::string& line : lines) {
            drawText(canvas, panel.x + 68.0f, y, line, body);
            y += 21.0f;
        }
        return;
    }

    // --- Toolbar: breadcrumbs -------------------------------------------
    float chipX = kContentX;
    const float chipH = 32.0f;
    const float chipY = kToolbarY;

    {
        std::string label = api.displayName();
        const float width = measureText(label, TextParams{Weight::Medium, 13.0f}) + 46.0f;
        drawChip(canvas, Rect{chipX, chipY, width, chipH}, label, sectionIcon(section_), true);
        chipX += width + 6.0f;
    }

    const std::vector<std::string> segments = pathSegments(view.path, api.root());
    for (std::size_t i = 0; i < segments.size(); ++i) {
        TextParams sep;
        sep.weight = Weight::Light;
        sep.size = 13.0f;
        sep.color = theme::kTextMuted;
        drawTextVCentered(canvas, chipX + 2.0f, chipY + chipH * 0.5f, "›", sep);
        chipX += 14.0f;

        const bool last = i + 1 == segments.size();
        const float width = measureText(segments[i], TextParams{Weight::Regular, 13.0f}) + 24.0f;
        drawChip(canvas, Rect{chipX, chipY, std::min(width, 220.0f), chipH},
                 segments[i].size() > 26 ? segments[i].substr(0, 24) + "…" : segments[i],
                 Icon::Close, last);
        chipX += std::min(width, 220.0f) + 6.0f;
    }

    if (!view.filter.empty()) {
        TextParams filterParams;
        filterParams.weight = Weight::Regular;
        filterParams.size = 12.0f;
        const float width = measureText("Filtre : " + view.filter, filterParams) + 24.0f;
        const Rect chip{chipX, chipY, width, chipH};
        canvas.fillRoundRect(chip, 10.0f, withAlpha(theme::kAccent, 0.16f));
        canvas.strokeRoundRect(chip, 10.0f, withAlpha(theme::kAccent, 0.45f), 1.0f);
        drawTextVCentered(canvas, chip.x + 12.0f, chip.y + chip.h * 0.5f,
                          "Filtre : " + view.filter, TextParams{Weight::Regular, 12.0f,
                                                                theme::kTextSecondary});
    }

    // Right side: sort + view.
    {
        const std::string sortText = sortLabel(view.sort, view.ascending);
        TextParams sortParams;
        sortParams.weight = Weight::Regular;
        sortParams.size = 12.0f;
        const float sortW = measureText(sortText, sortParams) + 44.0f;
        drawChip(canvas, Rect{kContentRight - sortW, chipY, sortW, chipH}, sortText, Icon::Sort,
                 false);

        const float viewW = 46.0f;
        drawChip(canvas, Rect{kContentRight - sortW - 8.0f - viewW, chipY, viewW, chipH}, "",
                 view.view == ViewMode::List ? Icon::Rows : Icon::Grid, false);
    }

    // --- Column header (list only) --------------------------------------
    if (view.view == ViewMode::List) {
        TextParams column;
        column.weight = Weight::Medium;
        column.size = 10.0f;
        column.color = hex(0x82828F);
        column.spacing = 1.4f;
        drawText(canvas, kContentX + 46.0f, kListHeaderY, "NOM", column);
        drawTextRight(canvas, kContentRight - 186.0f, kListHeaderY + 6.0f, "TAILLE", column);
        drawTextRight(canvas, kContentRight, kListHeaderY + 6.0f, "MODIFIÉ", column);
        canvas.fillRect(Rect{kContentX, kListHeaderY + 20.0f, kContentW, 1.0f},
                        withAlpha(theme::kBorder, 0.7f));
    }

    // --- Content ---------------------------------------------------------
    const Rect viewport{kContentX, kContentTop, kContentW, kVisibleListHeight};
    canvas.pushClip(viewport);

    if (!view.loaded) {
        TextParams loading;
        loading.weight = Weight::Light;
        loading.size = 14.0f;
        loading.color = theme::kTextMuted;
        drawTextCentered(canvas, viewport.x + viewport.w * 0.5f,
                         viewport.y + viewport.h * 0.5f, "Chargement…", loading);
        canvas.popClip();
        return;
    }

    if (!view.error.empty()) {
        drawIcon(canvas, Icon::Warning,
                 Rect{viewport.x + viewport.w * 0.5f - 16.0f, viewport.y + 90.0f, 32.0f, 32.0f},
                 theme::kDanger, 0.9f);
        TextParams error;
        error.weight = Weight::Regular;
        error.size = 14.0f;
        error.color = theme::kTextSecondary;
        drawTextCentered(canvas, viewport.x + viewport.w * 0.5f, viewport.y + 152.0f,
                         view.error, error);
        canvas.popClip();
        return;
    }

    if (view.entries.empty()) {
        drawIcon(canvas, Icon::FolderOpen,
                 Rect{viewport.x + viewport.w * 0.5f - 24.0f, viewport.y + 96.0f, 48.0f, 48.0f},
                 theme::kTextMuted, 0.55f);
        TextParams empty;
        empty.weight = Weight::Regular;
        empty.size = 15.0f;
        empty.color = theme::kTextSecondary;
        drawTextCentered(canvas, viewport.x + viewport.w * 0.5f, viewport.y + 170.0f,
                         view.filter.empty() ? "Ce dossier est vide" : "Aucun résultat",
                         empty);
        TextParams hint;
        hint.weight = Weight::Light;
        hint.size = 12.0f;
        hint.color = theme::kTextMuted;
        drawTextCentered(canvas, viewport.x + viewport.w * 0.5f, viewport.y + 194.0f,
                         "Y pour créer un dossier ou un fichier", hint);
        canvas.popClip();
        return;
    }

    const int count = static_cast<int>(view.entries.size());
    if (view.view == ViewMode::List) {
        for (int i = 0; i < count; ++i) {
            const float y = viewport.y + static_cast<float>(i) * (kRowHeight + kRowGap) - view.scroll;
            if (y > viewport.bottom() || y + kRowHeight < viewport.y) {
                continue;
            }
            const FileEntry& entry = view.entries[static_cast<std::size_t>(i)];
            const bool selected = view.selection.count(entry.name) > 0;
            const bool cursor = i == view.cursor;

            float reveal = 1.0f;
            if (settings_.animations && view.appear < 1.0f) {
                const float local = (view.appear - static_cast<float>(i) * 0.022f) / 0.28f;
                reveal = easeOutCubic(std::clamp(local, 0.0f, 1.0f));
            }
            const float offset = (1.0f - reveal) * 10.0f;

            const Rect row{viewport.x, y + offset, viewport.w, kRowHeight};
            if (cursor) {
                canvas.fillRoundRect(row, 10.0f, withAlpha(theme::kPanelHover, reveal));
                canvas.strokeRoundRect(row, 10.0f, withAlpha(theme::kBorderStrong, reveal), 1.0f);
            } else if (selected) {
                canvas.fillRoundRect(row, 10.0f, withAlpha(theme::kPanel, 0.85f * reveal));
            }

            const float iconSize = 19.0f;
            const Icon icon = iconForFile(entry.name.c_str(), entry.isDirectory);
            drawIcon(canvas, icon,
                     Rect{row.x + 16.0f, row.y + (kRowHeight - iconSize) * 0.5f, iconSize, iconSize},
                     entry.isDirectory ? theme::kText : theme::kTextSecondary, reveal);

            if (selected && !cursor) {
                canvas.fillCircle(row.x + 30.0f, row.y + kRowHeight - 12.0f, 2.5f,
                                  withAlpha(theme::kAccent, reveal));
            }

            TextParams name;
            name.weight = cursor ? Weight::Medium : Weight::Regular;
            name.size = 14.0f;
            name.color = cursor ? theme::kText : theme::kTextSecondary;
            name.opacity = reveal;
            const float maxName = viewport.w - 46.0f - 220.0f;
            const std::string label = gFonts->ellipsize(
                TextStyle{name.weight, name.size, name.color, 1.0f, 0.0f}, entry.name, maxName);
            drawTextVCentered(canvas, row.x + 46.0f, row.y + kRowHeight * 0.5f, label, name);

            TextParams meta;
            meta.weight = Weight::Light;
            meta.size = 12.0f;
            meta.color = theme::kTextMuted;
            meta.opacity = reveal;
            drawTextRight(canvas, row.right() - 186.0f, row.y + kRowHeight * 0.5f,
                          entry.isDirectory ? "—" : formatSize(entry.size), meta);
            drawTextRight(canvas, row.right() - 16.0f, row.y + kRowHeight * 0.5f,
                          formatDate(entry.modified), meta);
        }
    } else {
        for (int i = 0; i < count; ++i) {
            const int column = i % kGridColumns;
            const int rowIndex = i / kGridColumns;
            const float x = viewport.x + static_cast<float>(column) * (kGridCardW + kGridGap);
            const float y = viewport.y + static_cast<float>(rowIndex) * (kGridCardH + kGridGap) -
                            view.scroll;
            if (y > viewport.bottom() || y + kGridCardH < viewport.y) {
                continue;
            }
            const FileEntry& entry = view.entries[static_cast<std::size_t>(i)];
            const bool selected = view.selection.count(entry.name) > 0;
            const bool cursor = i == view.cursor;

            float reveal = 1.0f;
            if (settings_.animations && view.appear < 1.0f) {
                const float local = (view.appear - static_cast<float>(i) * 0.022f) / 0.28f;
                reveal = easeOutCubic(std::clamp(local, 0.0f, 1.0f));
            }

            const Rect card{x, y + (1.0f - reveal) * 12.0f, kGridCardW, kGridCardH};
            canvas.fillRoundRect(card, 14.0f, withAlpha(theme::kPanel, reveal));
            canvas.strokeRoundRect(card, 14.0f,
                                   withAlpha(cursor ? theme::kBorderStrong : theme::kBorder, reveal),
                                   cursor ? 1.5f : 1.0f);
            if (cursor) {
                canvas.fillRoundRect(Rect{card.x, card.y + 14.0f, 3.0f, card.h - 28.0f}, 1.5f,
                                     withAlpha(rainbow(time_ * 0.25f), reveal));
            }

            drawIcon(canvas, iconForFile(entry.name.c_str(), entry.isDirectory),
                     Rect{card.x + card.w * 0.5f - 20.0f, card.y + 22.0f, 40.0f, 40.0f},
                     entry.isDirectory ? theme::kText : theme::kTextSecondary, reveal);

            TextParams name;
            name.weight = Weight::Medium;
            name.size = 12.5f;
            name.color = theme::kText;
            name.opacity = reveal;
            const std::string label = gFonts->ellipsize(
                TextStyle{name.weight, name.size, name.color, 1.0f, 0.0f}, entry.name,
                card.w - 24.0f);
            drawTextCentered(canvas, card.x + card.w * 0.5f, card.y + 82.0f, label, name);

            TextParams meta;
            meta.weight = Weight::Light;
            meta.size = 11.0f;
            meta.color = theme::kTextMuted;
            meta.opacity = reveal;
            drawTextCentered(canvas, card.x + card.w * 0.5f, card.y + 104.0f,
                             entry.isDirectory ? "Dossier" : formatSize(entry.size), meta);

            if (selected) {
                canvas.fillCircle(card.right() - 18.0f, card.y + 18.0f, 8.0f,
                                  withAlpha(theme::kAccent, reveal));
                drawIcon(canvas, Icon::Check,
                         Rect{card.right() - 23.0f, card.y + 13.0f, 10.0f, 10.0f},
                         hex(0x0A0A0C), reveal);
            }
        }
    }

    canvas.popClip();
}

// ---------------------------------------------------------------------------
// FTP
// ---------------------------------------------------------------------------
void App::renderFtp(Canvas& canvas) {
    const bool running = ftpState_.running;

    const Rect card{kContentX, kContentTop - 20.0f, kContentW, 176.0f};
    drawCard(canvas, card, 18.0f, theme::kPanel);

    drawIcon(canvas, Icon::Network, Rect{card.x + 26.0f, card.y + 26.0f, 24.0f, 24.0f},
             running ? theme::kSuccess : theme::kTextSecondary);

    TextParams title;
    title.weight = Weight::Medium;
    title.size = 19.0f;
    drawText(canvas, card.x + 62.0f, card.y + 26.0f, "Serveur FTP", title);

    const float pulse = 0.55f + 0.45f * std::sin(pulse_ * 3.0f);
    drawPill(canvas, Rect{card.x + 62.0f, card.y + 58.0f, 108.0f, 26.0f},
             running ? "En ligne" : "Arrêté",
             running ? withAlpha(theme::kSuccess, pulse) : theme::kTextMuted,
             running ? theme::kText : theme::kTextSecondary);

    if (running) {
        TextParams address;
        address.weight = Weight::Regular;
        address.size = 17.0f;
        drawText(canvas, card.x + 62.0f, card.y + 96.0f,
                 "ftp://" + ftpState_.address, address);

        TextParams credentials;
        credentials.weight = Weight::Light;
        credentials.size = 12.0f;
        credentials.color = theme::kTextMuted;
        drawText(canvas, card.x + 62.0f, card.y + 128.0f,
                 "Utilisateur " + settings_.ftpUser + "   ·   Mot de passe " +
                     settings_.ftpPassword + "   ·   " +
                     std::to_string(ftpState_.clients) + " client(s)",
                 credentials);
    } else {
        TextParams hint;
        hint.weight = Weight::Light;
        hint.size = 13.0f;
        hint.color = theme::kTextSecondary;
        drawText(canvas, card.x + 62.0f, card.y + 98.0f,
                 "Accédez aux fichiers de la console depuis un autre appareil du réseau.", hint);
        TextParams port;
        port.weight = Weight::Light;
        port.size = 12.0f;
        port.color = theme::kTextMuted;
        drawText(canvas, card.x + 62.0f, card.y + 124.0f,
                 "Port " + std::to_string(settings_.ftpPort) + "   ·   " + settings_.ftpUser +
                     " / " + settings_.ftpPassword,
                 port);
    }

    const Rect button{card.right() - 210.0f, card.y + 64.0f, 178.0f, 48.0f};
    drawButton(canvas, button, running ? "Arrêter" : "Démarrer", !running, false, 1.0f);

    // Log
    const Rect logCard{kContentX, card.bottom() + 16.0f, kContentW, kContentBottom - card.bottom() - 16.0f};
    drawCard(canvas, logCard, 18.0f, theme::kPanel);

    TextParams logTitle;
    logTitle.weight = Weight::Medium;
    logTitle.size = 11.0f;
    logTitle.color = theme::kTextMuted;
    logTitle.spacing = 1.6f;
    drawText(canvas, logCard.x + 26.0f, logCard.y + 20.0f, "ACTIVITÉ", logTitle);

    canvas.pushClip(Rect{logCard.x + 26.0f, logCard.y + 44.0f, logCard.w - 52.0f,
                         logCard.h - 60.0f});
    float y = logCard.y + 46.0f;
    if (ftpState_.log.empty()) {
        TextParams empty;
        empty.weight = Weight::Light;
        empty.size = 12.0f;
        empty.color = theme::kTextMuted;
        drawText(canvas, logCard.x + 26.0f, y,
                 running ? "En attente de connexion…" : "Le serveur est arrêté.", empty);
    } else {
        const int total = static_cast<int>(ftpState_.log.size());
        const int first = std::max(0, total - 11);
        for (int i = first; i < total; ++i) {
            TextParams line;
            line.weight = Weight::Light;
            line.size = 12.0f;
            line.color = theme::kTextSecondary;
            const std::string& entry = ftpState_.log[static_cast<std::size_t>(i)];
            drawText(canvas, logCard.x + 26.0f, y,
                     gFonts->ellipsize(TextStyle{Weight::Light, 12.0f}, entry,
                                       logCard.w - 52.0f),
                     line);
            y += 19.0f;
        }
    }
    canvas.popClip();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void App::renderApps(Canvas& canvas) {
    constexpr float kRowHeight = 64.0f;
    constexpr float kStride = 70.0f;

    if (!apps_.error.empty() || apps_.entries.empty()) {
        const bool blocked = !apps_.error.empty();
        drawIcon(canvas, blocked ? Icon::Warning : Icon::Apps,
                 Rect{kContentX + kContentW * 0.5f - 22.0f, kContentTop + 96.0f, 44.0f, 44.0f},
                 blocked ? theme::kDanger : theme::kTextMuted, 0.9f);

        TextParams title;
        title.weight = Weight::Medium;
        title.size = 17.0f;
        title.opacity = 0.92f;
        drawTextCentered(canvas, kContentX + kContentW * 0.5f, kContentTop + 166.0f,
                         blocked ? "Applications indisponibles" : "Aucune application installée",
                         title);

        if (blocked) {
            TextParams message;
            message.weight = Weight::Light;
            message.size = 13.0f;
            message.color = theme::kTextMuted;
            const std::vector<std::string> lines =
                wrapText(apps_.error, message, kContentW - 200.0f);
            float y = kContentTop + 202.0f;
            for (const std::string& line : lines) {
                drawTextCentered(canvas, kContentX + kContentW * 0.5f, y, line, message);
                y += 20.0f;
            }
        }
        return;
    }

    TextParams column;
    column.weight = Weight::Medium;
    column.size = 10.0f;
    column.color = hex(0x82828F);
    column.spacing = 1.4f;
    drawText(canvas, kContentX + 72.0f, kListHeaderY, "APPLICATION", column);
    drawTextRight(canvas, kContentRight, kListHeaderY + 7.0f, "VERSION", column);

    canvas.pushClip(
        Rect{kContentX - 8.0f, kContentTop, kContentW + 16.0f, kContentBottom - kContentTop});

    const int count = static_cast<int>(apps_.entries.size());
    const int first = std::max(0, static_cast<int>(apps_.scroll / kStride) - 1);
    for (int i = first; i < count; ++i) {
        const float y = kContentTop + static_cast<float>(i) * kStride - apps_.scroll;
        if (y > kContentBottom) {
            break;
        }
        if (y + kRowHeight < kContentTop - kStride) {
            continue;
        }

        const TitleEntry& entry = apps_.entries[static_cast<std::size_t>(i)];
        const bool selected = i == apps_.cursor;
        const Rect row{kContentX, y, kContentW, kRowHeight};

        if (selected) {
            canvas.fillRoundRect(row, 14.0f, theme::kPanelActive);
            canvas.strokeRoundRect(row, 14.0f, theme::kBorderStrong, 1.0f);
            canvas.fillRoundRect(Rect{row.x + 1.0f, y + 20.0f, 3.0f, 24.0f}, 1.5f,
                                 rainbow(time_ * 0.25f));
        } else if (i % 2 == 1) {
            canvas.fillRoundRect(row, 14.0f, withAlpha(hex(0x0B0B0E), 0.35f));
        }

        const Rect iconBox{row.x + 16.0f, y + (kRowHeight - 44.0f) * 0.5f, 44.0f, 44.0f};
        const Image* icon = titleIcon(entry);
        if (icon != nullptr) {
            canvas.drawImageRounded(*icon, iconBox, 12.0f, 1.0f);
        } else {
            canvas.fillRoundRect(iconBox, 12.0f, hex(0x1D1D24));
            drawIcon(canvas, Icon::Apps, Rect{iconBox.x + 11.0f, iconBox.y + 11.0f, 22.0f, 22.0f},
                     theme::kTextMuted, 0.7f);
        }

        TextParams name;
        name.weight = selected ? Weight::Medium : Weight::Regular;
        name.size = 14.0f;
        name.color = selected ? theme::kText : hex(0xE8E8F0);
        drawText(canvas, iconBox.right() + 16.0f, y + 13.0f,
                 gFonts->ellipsize(TextStyle{name.weight, name.size}, entry.name, kContentW - 420.0f),
                 name);

        char details[256];
        std::snprintf(details, sizeof(details), "%016llX · %s%s%s",
                      static_cast<unsigned long long>(entry.id),
                      entry.author.empty() ? "Éditeur inconnu" : entry.author.c_str(),
                      entry.updates > 0 ? " · Mise à jour" : "",
                      entry.dlc > 0 ? (" · " + std::to_string(entry.dlc) + " DLC").c_str() : "");
        TextParams meta;
        meta.weight = Weight::Light;
        meta.size = 11.0f;
        meta.color = theme::kTextMuted;
        drawText(canvas, iconBox.right() + 16.0f, y + 36.0f,
                 gFonts->ellipsize(TextStyle{Weight::Light, 11.0f}, details, kContentW - 420.0f),
                 meta);

        TextParams version;
        version.weight = Weight::Regular;
        version.size = 12.0f;
        version.color = selected ? theme::kTextSecondary : hex(0x9A9AA8);
        drawTextRight(canvas, kContentRight - 16.0f, y + kRowHeight * 0.5f,
                      entry.version.empty() ? "—" : entry.version, version);
    }

    canvas.popClip();
}

void App::renderSettings(Canvas& canvas) {
    struct Row {
        const char* label;
        const char* hint;
        Icon icon;
    };
    const Row rows[] = {
        {"Animations", "Transitions et apparitions", Icon::Power},
        {"Vue par défaut", "Explorateur", Icon::Grid},
        {"Port du serveur FTP", "Actif au prochain démarrage", Icon::Network},
        {"Utilisateur FTP", "Identifiant", Icon::Link},
        {"Mot de passe FTP", "Mot de passe", Icon::Settings},
    };
    constexpr int kRowCount = 5;

    const Rect card{kContentX, kContentTop - 20.0f, kContentW, 64.0f * kRowCount + 16.0f};
    drawCard(canvas, card, 18.0f, theme::kPanel);

    for (int i = 0; i < kRowCount; ++i) {
        const float y = card.y + 8.0f + static_cast<float>(i) * 64.0f;
        const bool focused = settingsCursor_ == i;
        if (focused) {
            canvas.fillRoundRect(Rect{card.x + 8.0f, y, card.w - 16.0f, 58.0f}, 12.0f,
                                 theme::kPanelHover);
            canvas.strokeRoundRect(Rect{card.x + 8.0f, y, card.w - 16.0f, 58.0f}, 12.0f,
                                   theme::kBorderStrong, 1.0f);
        }

        drawIcon(canvas, rows[i].icon, Rect{card.x + 26.0f, y + 18.0f, 20.0f, 20.0f},
                 focused ? theme::kText : theme::kTextSecondary);

        TextParams label;
        label.weight = Weight::Medium;
        label.size = 14.0f;
        drawText(canvas, card.x + 60.0f, y + 12.0f, rows[i].label, label);

        TextParams hint;
        hint.weight = Weight::Light;
        hint.size = 11.0f;
        hint.color = theme::kTextMuted;
        drawText(canvas, card.x + 60.0f, y + 33.0f, rows[i].hint, hint);

        std::string value;
        switch (i) {
            case 0: value = settings_.animations ? "Activées" : "Désactivées"; break;
            case 1: value = settings_.gridDefault ? "Grille" : "Liste"; break;
            case 2: value = std::to_string(settings_.ftpPort); break;
            case 3: value = settings_.ftpUser; break;
            case 4: value = settings_.ftpPassword; break;
            default: break;
        }

        if (i == 0) {
            drawToggle(canvas, Rect{card.right() - 76.0f, y + 17.0f, 44.0f, 24.0f},
                       settings_.animations);
        } else {
            TextParams valueParams;
            valueParams.weight = Weight::Regular;
            valueParams.size = 13.0f;
            valueParams.color = focused ? theme::kText : theme::kTextSecondary;
            drawTextRight(canvas, card.right() - 44.0f, y + 29.0f, value, valueParams);
            drawIcon(canvas, Icon::ChevronRight,
                     Rect{card.right() - 32.0f, y + 21.0f, 14.0f, 14.0f}, theme::kTextMuted);
        }
    }

    TextParams note;
    note.weight = Weight::Light;
    note.size = 12.0f;
    note.color = theme::kTextMuted;
    drawText(canvas, kContentX + 4.0f, card.bottom() + 16.0f,
             "A pour modifier · les réglages FTP sont enregistrés sur la carte SD.", note);
}

// ---------------------------------------------------------------------------
// About
// ---------------------------------------------------------------------------
void App::renderAbout(Canvas& canvas) {
    const float top = kContentTop - 20.0f;

    if (logo_ != nullptr && logo_->valid()) {
        canvas.drawImageRounded(*logo_, Rect{kContentX, top, 92.0f, 92.0f}, 22.0f, 1.0f);
    }

    TextParams name;
    name.weight = Weight::Medium;
    name.size = 26.0f;
    drawText(canvas, kContentX + 112.0f, top + 6.0f, "NXF Pro", name);

    TextParams version;
    version.weight = Weight::Light;
    version.size = 13.0f;
    version.color = theme::kTextSecondary;
    drawText(canvas, kContentX + 112.0f, top + 44.0f, "Version 1.1.0  ·  homebrew pour Nintendo Switch",
             version);

    TextParams body;
    body.weight = Weight::Light;
    body.size = 13.5f;
    body.color = theme::kTextSecondary;
    const std::vector<std::string> lines = wrapText(
        "Gestionnaire de fichiers natif avec serveur FTP intégré. Interface sombre, "
        "typographie Montserrat, dégradés animés, et opérations réelles sur la carte SD "
        "et la mémoire interne.",
        body, kContentW - 20.0f);
    float y = top + 124.0f;
    for (const std::string& line : lines) {
        drawText(canvas, kContentX, y, line, body);
        y += 21.0f;
    }

    const Rect card{kContentX, y + 18.0f, kContentW, 210.0f};
    drawCard(canvas, card, 18.0f, theme::kPanel);

    TextParams heading;
    heading.weight = Weight::Medium;
    heading.size = 11.0f;
    heading.color = theme::kTextMuted;
    heading.spacing = 1.6f;
    drawText(canvas, card.x + 26.0f, card.y + 22.0f, "COMPOSANTS", heading);

    struct Component {
        const char* name;
        const char* licence;
    };
    const Component components[] = {
        {"libnx", "ISC · devkitPro"},
        {"Montserrat", "SIL Open Font License 1.1"},
        {"stb_truetype / stb_image", "domaine public · Sean Barrett"},
        {"devkitA64", "GCC pour Nintendo Switch"},
    };

    float rowY = card.y + 56.0f;
    for (const Component& component : components) {
        TextParams label;
        label.weight = Weight::Regular;
        label.size = 13.0f;
        drawText(canvas, card.x + 26.0f, rowY, component.name, label);

        TextParams licence;
        licence.weight = Weight::Light;
        licence.size = 12.0f;
        licence.color = theme::kTextMuted;
        drawTextRight(canvas, card.right() - 26.0f, rowY + 8.0f, component.licence, licence);
        rowY += 34.0f;
    }
}

// ---------------------------------------------------------------------------
// Footer
// ---------------------------------------------------------------------------
void App::renderFooter(Canvas& canvas) {
    canvas.fillRect(Rect{kContentX, kFooterY - 12.0f, kContentW, 1.0f},
                    withAlpha(theme::kBorder, 0.7f));

    TextParams hint;
    hint.weight = Weight::Light;
    hint.size = 11.0f;
    hint.color = theme::kTextMuted;

    std::string hints;
    switch (section_) {
        case Section::Sd:
        case Section::Internal:
            hints = "A Ouvrir    B Retour    X Sélection    Y Actions    L Vue    R Tri    "
                    "− Rechercher    ZL/ZR Sections    + Quitter";
            break;
        case Section::Apps:
            hints = "A Supprimer définitivement    ↑↓ Navigation    ZL/ZR Sections    + Quitter";
            break;
        case Section::Ftp:
            hints = "A Démarrer / Arrêter    ZL/ZR Sections    + Quitter";
            break;
        case Section::Settings:
            hints = "↑↓ Navigation    A Modifier    ZL/ZR Sections    + Quitter";
            break;
        case Section::About:
            hints = "ZL/ZR Sections    + Quitter";
            break;
    }
    drawText(canvas, kContentX, kFooterY, hints, hint);

    if (section_ == Section::Sd || section_ == Section::Internal) {
        ExplorerState& view = this->view();
        char buffer[128];
        const int count = static_cast<int>(view.entries.size());
        if (view.selection.empty()) {
            std::snprintf(buffer, sizeof(buffer), "%d élément%s", count, count == 1 ? "" : "s");
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d élément%s · %d sélectionné%s", count,
                          count == 1 ? "" : "s", static_cast<int>(view.selection.size()),
                          view.selection.size() == 1 ? "" : "s");
        }
        TextParams stats;
        stats.weight = Weight::Light;
        stats.size = 11.0f;
        stats.color = theme::kTextMuted;
        drawTextRight(canvas, kContentRight, kFooterY + 7.0f, buffer, stats);
    }

    if (section_ == Section::Apps) {
        char buffer[64];
        const int count = static_cast<int>(apps_.entries.size());
        std::snprintf(buffer, sizeof(buffer), "%d application%s", count, count == 1 ? "" : "s");
        TextParams stats;
        stats.weight = Weight::Light;
        stats.size = 11.0f;
        stats.color = theme::kTextMuted;
        drawTextRight(canvas, kContentRight, kFooterY + 7.0f, buffer, stats);
    }
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------
void App::renderDialog(Canvas& canvas) {
    if (dialog_ == DialogKind::None) {
        return;
    }
    canvas.dim(0.55f * dialogAnim_);

    const float scale = 0.96f + 0.04f * easeOutCubic(dialogAnim_);
    const float opacity = easeOutCubic(dialogAnim_);

    if (dialog_ == DialogKind::Actions) {
        const int itemCount = static_cast<int>(dialogActions_.size());
        const float height = 22.0f + static_cast<float>(itemCount) * 44.0f + 12.0f;
        const Rect panel{kContentRight - 330.0f, kContentTop + 40.0f - (1.0f - scale) * 20.0f,
                         330.0f, height};
        canvas.dropShadow(panel, 16.0f, 22.0f, 0.55f * opacity);
        canvas.fillRoundRect(panel, 16.0f, withAlpha(hex(0x14141A), opacity));
        canvas.strokeRoundRect(panel, 16.0f, withAlpha(theme::kBorderStrong, opacity), 1.0f);

        float y = panel.y + 12.0f;
        for (int i = 0; i < itemCount; ++i) {
            const bool focused = i == dialogCursor_;
            const Rect row{panel.x + 8.0f, y, panel.w - 16.0f, 40.0f};
            if (focused) {
                canvas.fillRoundRect(row, 10.0f, withAlpha(theme::kPanelActive, opacity));
            }
            const bool danger = dialogActions_[static_cast<std::size_t>(i)] == "Supprimer";
            TextParams label;
            label.weight = Weight::Regular;
            label.size = 13.0f;
            label.color = danger ? theme::kDanger : theme::kText;
            label.opacity = opacity;
            drawTextVCentered(canvas, row.x + 14.0f, row.y + row.h * 0.5f,
                              dialogActions_[static_cast<std::size_t>(i)], label);
            y += 44.0f;
        }
        return;
    }

    if (dialog_ == DialogKind::Confirm) {
        const Rect panel{kContentX + (kContentW - 560.0f) * 0.5f,
                         kContentTop + 100.0f - (1.0f - scale) * 24.0f, 560.0f, 214.0f};
        canvas.dropShadow(panel, 18.0f, 26.0f, 0.65f * opacity);
        canvas.fillRoundRect(panel, 18.0f, withAlpha(hex(0x131319), opacity));
        canvas.strokeRoundRect(panel, 18.0f, withAlpha(theme::kBorderStrong, opacity), 1.0f);

        drawIcon(canvas, Icon::Warning, Rect{panel.x + 28.0f, panel.y + 28.0f, 22.0f, 22.0f},
                 theme::kDanger, opacity);

        TextParams title;
        title.weight = Weight::Medium;
        title.size = 17.0f;
        title.opacity = opacity;
        drawText(canvas, panel.x + 62.0f, panel.y + 26.0f, dialogTitle_, title);

        TextParams message;
        message.weight = Weight::Light;
        message.size = 13.0f;
        message.color = theme::kTextSecondary;
        message.opacity = opacity;
        const std::vector<std::string> lines = wrapText(dialogMessage_, message, panel.w - 88.0f);
        float y = panel.y + 68.0f;
        for (const std::string& line : lines) {
            drawText(canvas, panel.x + 28.0f, y, line, message);
            y += 20.0f;
        }

        const Rect cancel{panel.x + panel.w - 352.0f, panel.bottom() - 66.0f, 160.0f, 44.0f};
        const Rect confirm{panel.x + panel.w - 180.0f, panel.bottom() - 66.0f, 152.0f, 44.0f};
        drawButton(canvas, cancel, "Annuler", false, dialogFocus_ == 0, opacity);
        drawButton(canvas, confirm, pendingAction_ == 2 ? "Supprimer" : "Confirmer", true,
                   dialogFocus_ == 1, opacity, pendingAction_ == 2);
        return;
    }

    // Properties
    const Rect panel{kContentX + (kContentW - 620.0f) * 0.5f,
                     kContentTop + 36.0f - (1.0f - scale) * 24.0f, 620.0f, 360.0f};
    canvas.dropShadow(panel, 18.0f, 26.0f, 0.65f * opacity);
    canvas.fillRoundRect(panel, 18.0f, withAlpha(hex(0x131319), opacity));
    canvas.strokeRoundRect(panel, 18.0f, withAlpha(theme::kBorderStrong, opacity), 1.0f);

    TextParams title;
    title.weight = Weight::Medium;
    title.size = 17.0f;
    title.opacity = opacity;
    drawText(canvas, panel.x + 28.0f, panel.y + 22.0f, "Propriétés", title);

    drawIcon(canvas, Icon::Close, Rect{panel.right() - 46.0f, panel.y + 24.0f, 15.0f, 15.0f},
             theme::kTextMuted, opacity);

    canvas.fillRect(Rect{panel.x + 28.0f, panel.y + 64.0f, panel.w - 56.0f, 1.0f},
                    withAlpha(theme::kBorder, 0.9f * opacity));

    const char* labels[5] = {"Nom", "Type", "Taille", "Modifié le", "Chemin"};
    for (int i = 0; i < 5; ++i) {
        const float rowY = panel.y + 84.0f + static_cast<float>(i) * 46.0f;

        TextParams label;
        label.weight = Weight::Light;
        label.size = 12.0f;
        label.color = theme::kTextMuted;
        label.opacity = opacity;
        drawTextVCentered(canvas, panel.x + 28.0f, rowY + 10.0f, labels[i], label);

        TextParams value;
        value.weight = Weight::Regular;
        value.size = 13.0f;
        value.opacity = opacity;
        const std::string text =
            i < static_cast<int>(dialogActions_.size()) ? dialogActions_[static_cast<std::size_t>(i)]
                                                        : std::string();
        const float maxWidth = panel.w - 240.0f;
        drawTextRight(canvas, panel.right() - 28.0f, rowY + 10.0f,
                      gFonts->ellipsize(TextStyle{Weight::Regular, 13.0f}, text, maxWidth), value);

        if (i < 4) {
            canvas.fillRect(Rect{panel.x + 28.0f, rowY + 32.0f, panel.w - 56.0f, 1.0f},
                            withAlpha(theme::kBorder, 0.5f * opacity));
        }
    }

    const Rect close{panel.right() - 150.0f, panel.bottom() - 60.0f, 122.0f, 40.0f};
    drawButton(canvas, close, "Fermer", false, true, opacity);
}

void App::renderToast(Canvas& canvas) {
    if (toast_.empty() || toastTimer_ <= 0.0f) {
        return;
    }
    const float appear = std::clamp((2.6f - toastTimer_) / 0.22f, 0.0f, 1.0f);
    const float opacity = easeOutCubic(appear);
    const float offset = (1.0f - opacity) * 18.0f;

    TextParams params;
    params.weight = Weight::Regular;
    params.size = 13.0f;
    const float width = measureText(toast_, params) + 72.0f;
    const Rect toast{kContentX + (kContentW - width) * 0.5f, kContentBottom - 66.0f + offset,
                     width, 44.0f};
    canvas.fillRoundRect(toast, 12.0f, withAlpha(hex(0x1B1B23), opacity));
    canvas.strokeRoundRect(toast, 12.0f, withAlpha(theme::kBorderStrong, opacity), 1.0f);
    drawIcon(canvas, Icon::Check, Rect{toast.x + 16.0f, toast.y + 15.0f, 14.0f, 14.0f},
             theme::kSuccess, opacity);
    params.opacity = opacity;
    drawTextVCentered(canvas, toast.x + 40.0f, toast.y + toast.h * 0.5f, toast_, params);
}

} // namespace nxf
