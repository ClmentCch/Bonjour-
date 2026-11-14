/*
 * NXF Pro - language selection and interface strings.
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <string>

namespace nxf {

enum class Lang {
    English = 0,
    French = 1,
    Spanish = 2,
    German = 3,
    Italian = 4,
    Portuguese = 5,
    Dutch = 6,
    Polish = 7,
    Russian = 8,
    Japanese = 9,
    Korean = 10,
    Chinese = 11,
};

constexpr int kLangCount = 12;

struct LangInfo {
    Lang lang;
    const char* code;
    const char* native;
};

const LangInfo& langInfo(int index);
int langIndex(Lang lang);
Lang langFromCode(const std::string& code);
const char* langCode(Lang lang);
const char* langNative(Lang lang);

enum class Txt {
    // Sidebar
    NavSd,
    NavInternal,
    NavApps,
    NavDbi,
    NavFtp,
    NavSettings,
    NavAbout,
    // Section subtitles
    SubSd,
    SubInternal,
    SubApps,
    SubDbi,
    SubFtp,
    SubSettings,
    SubAbout,
    // First run
    ChooseLanguage,
    SaveLanguage,
    LanguageNote,
    LanguageSaved,
    // DBI
    StartFtp,
    StopFtp,
    InstallNsp,
    InstalledContent,
    Storage,
    NspFiles,
    NoNspFound,
    Scanning,
    Back,
    Yes,
    No,
    RemoveAppContent,
    InstallUnavailable,
    InstallCancelledKept,
    CancelNoRemove,
    Installed,
    NotInstalled,
    DbiHint,
    Items,
};

/// Text in the currently selected language.
const char* tr(Txt key);

void setLang(Lang lang);
Lang lang();

} // namespace nxf
