/*
 * NXF Pro - language table.
 *
 * English and French are translated; the other languages fall back to English
 * until their strings are added.
 *
 * MIT licensed, see LICENSE.
 */
#include "nxf/i18n.hpp"

#include <cstring>

namespace nxf {
namespace {

const LangInfo kLanguages[kLangCount] = {
    {Lang::English, "en", "English"},
    {Lang::French, "fr", "Français"},
    {Lang::Spanish, "es", "Español"},
    {Lang::German, "de", "Deutsch"},
    {Lang::Italian, "it", "Italiano"},
    {Lang::Portuguese, "pt", "Português"},
    {Lang::Dutch, "nl", "Nederlands"},
    {Lang::Polish, "pl", "Polski"},
    {Lang::Russian, "ru", "Russian"},
    {Lang::Japanese, "ja", "Japanese"},
    {Lang::Korean, "ko", "Korean"},
    {Lang::Chinese, "zh", "Chinese"},
};

const char* kEnglish[] = {
    "Switch SD", "Internal memory", "Applications", "DBI", "FTP", "Settings", "About",
    "SD card", "Internal memory", "Applications", "DBI", "FTP server", "Settings", "About",
    "Choose your language :", "Save", "Interface translated in your language (more coming).",
    "Language saved", 
    "Start FTP Server", "Stop FTP Server", "Install an NSP", "Installed content", "Storage",
    "NSP files", "No .nsp file found on the SD card", "Scanning the SD card...", "Back",
    "Yes", "No", "Delete this app's content?", "NSP installation is not available in this build.",
    "Installation cancelled: content kept.",
    "Installation cancelled because the content was not removed.", "Installed", "Not installed",
    "A Démarrer / Arrêter    B Retour    ZL/ZR Sections    + Quitter", "items",
};

const char* kFrench[] = {
    "Switch SD", "Mémoire interne", "Applications", "DBI", "FTP", "Paramètres", "À propos",
    "Carte SD", "Mémoire interne", "Applications", "DBI", "Serveur FTP", "Paramètres", "À propos",
    "Choose your language :", "Enregistrer",
    "Interface traduite dans votre langue (d'autres arrivent).", "Langue enregistrée",
    "Démarrer le serveur FTP", "Arrêter le serveur FTP", "Installer un NSP", "Contenu installé",
    "Stockage",
    "Fichiers NSP", "Aucun fichier .nsp sur la carte SD", "Analyse de la carte SD...", "Retour",
    "Oui", "Non", "Supprimer le contenu de cette app ?",
    "L'installation de NSP n'est pas disponible dans cette version.",
    "Installation annulée : contenu conservé.",
    "Installation annulé car contenu non supprimé.", "Installé", "Non installé",
    "A Démarrer / Arrêter    B Retour    ZL/ZR Sections    + Quitter", "éléments",
};

Lang gLang = Lang::French;

} // namespace

const LangInfo& langInfo(int index) {
    if (index < 0 || index >= kLangCount) {
        return kLanguages[0];
    }
    return kLanguages[index];
}

int langIndex(Lang lang) {
    for (int i = 0; i < kLangCount; ++i) {
        if (kLanguages[i].lang == lang) {
            return i;
        }
    }
    return 0;
}

Lang langFromCode(const std::string& code) {
    for (const LangInfo& info : kLanguages) {
        if (code == info.code) {
            return info.lang;
        }
    }
    return Lang::English;
}

const char* langCode(Lang lang) {
    return langInfo(langIndex(lang)).code;
}

const char* langNative(Lang lang) {
    return langInfo(langIndex(lang)).native;
}

const char* tr(Txt key) {
    const int index = static_cast<int>(key);
    const int count = static_cast<int>(sizeof(kEnglish) / sizeof(kEnglish[0]));
    if (index < 0 || index >= count) {
        return "";
    }
    if (gLang == Lang::French) {
        return kFrench[index];
    }
    return kEnglish[index];
}

void setLang(Lang lang) { gLang = lang; }
Lang lang() { return gLang; }

} // namespace nxf
