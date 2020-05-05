#include "boxedwine.h"
#include "../boxedwineui.h"

#include "../../io/fszip.h"
#include "../../util/networkutils.h"
#include "../../util/threadutils.h"
#include "../../../lib/pugixml/src/pugixml.hpp"

#include <sys/stat.h>

#include <SDL.h>

#define SLOW_FRAME_DELAY 1000
#define FAST_FRAME_DELAY 1

std::string GlobalSettings::dataFolderLocation;
std::vector<WineVersion> GlobalSettings::wineVersions;
std::vector<WineVersion> GlobalSettings::availableWineVersions;
std::vector<AppFile> GlobalSettings::demos;
std::vector<AppFile> GlobalSettings::components;
int GlobalSettings::iconSize;
StartUpArgs GlobalSettings::startUpArgs;
std::string GlobalSettings::exePath;
std::string GlobalSettings::exeFilePath;
std::string GlobalSettings::theme;
std::string GlobalSettings::configFilePath;
ImFont* GlobalSettings::largeFontBold;
ImFont* GlobalSettings::largeFont;
ImFont* GlobalSettings::mediumFont;
ImFont* GlobalSettings::defaultFont;
ImFont* GlobalSettings::sectionTitleFont;
U32 GlobalSettings::scale=1000;
bool GlobalSettings::filesListDownloading;
bool GlobalSettings::restartUI;
U32 GlobalSettings::frameDelayMillies = SLOW_FRAME_DELAY;
U32 GlobalSettings::fastFrameRateCount = 0;
U64 GlobalSettings::lastFrameDelayChange = 0;
std::vector<std::string> GlobalSettings::availableResolutions;
std::string GlobalSettings::defaultResolution;
int GlobalSettings::defaultScale;
int GlobalSettings::screenCx;
int GlobalSettings::screenCy;
float GlobalSettings::extraVerticalSpacing;
float GlobalSettings::fontScale;
bool GlobalSettings::iconFontsLoaded;
std::string GlobalSettings::filesUrl;

void GlobalSettings::init(int argc, const char **argv) {
    GlobalSettings::dataFolderLocation = SDL_GetPrefPath("", "Boxedwine");
    if (Fs::nativePathSeperator.length()==0) {
        Fs::nativePathSeperator = GlobalSettings::dataFolderLocation[GlobalSettings::dataFolderLocation.length()-1];
    }
    GlobalSettings::dataFolderLocation = GlobalSettings::dataFolderLocation.substr(0, GlobalSettings::dataFolderLocation.length()-1);
    GlobalSettings::exePath = Fs::getNativeParentPath(argv[0]);
    GlobalSettings::exeFilePath = argv[0];
    if (!Fs::doesNativePathExist(GlobalSettings::dataFolderLocation)) {
        Fs::makeNativeDirs(GlobalSettings::dataFolderLocation);
    }
    GlobalSettings::configFilePath = GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "boxedwine.ini";
    ConfigFile config(GlobalSettings::configFilePath);
    GlobalSettings::dataFolderLocation = config.readString("DataFolder", GlobalSettings::dataFolderLocation);
    stringReplaceAll(GlobalSettings::dataFolderLocation, "//", "/");
    stringReplaceAll(GlobalSettings::dataFolderLocation, "\\\\", "\\");
    GlobalSettings::theme = config.readString("Theme", "Dark");
    GlobalSettings::defaultResolution = config.readString("DefaultResolution", "1024x768");
    GlobalSettings::defaultScale = config.readInt("DefaultScale", 100);
    GlobalSettings::fontScale = (float)config.readInt("FontScale", 100) / 100.0f;
    GlobalSettings::filesUrl = config.readString("FilesURL", "http://www.boxedwine.org/files.xml");
    if (!Fs::doesNativePathExist(configFilePath)) {
        saveConfig();
    }    
    GlobalSettings::startUp();

    SDL_DisplayMode dm;    
    availableResolutions.push_back("640x480");
    availableResolutions.push_back("800x600");
    availableResolutions.push_back("1024x768");
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        GlobalSettings::screenCx = dm.w;
        GlobalSettings::screenCy = dm.h;
        if (dm.w>=3072) {
            std::string res = std::to_string(dm.w / 3) + "x" + std::to_string(dm.h / 3);
            availableResolutions.push_back(res.c_str());
        }
        if (dm.w>=2048) {
            std::string res = std::to_string(dm.w/2) + "x" + std::to_string(dm.h/2);
            availableResolutions.push_back(res.c_str());
        }
        std::string res = std::to_string(dm.w) + "x" + std::to_string(dm.h);
        availableResolutions.push_back(res.c_str());
    }
    GlobalSettings::extraVerticalSpacing = (float)GlobalSettings::scaleIntUI(5);
}

void GlobalSettings::startUp() {
    GlobalSettings::initWineVersions();
    std::string containersPath = GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "Containers";
    if (!Fs::doesNativePathExist(containersPath) && GlobalSettings::wineVersions.size() > 0) {
        std::string defaultContainerPath = containersPath + Fs::nativePathSeperator + "Default";
        Fs::makeNativeDirs(defaultContainerPath);
        BoxedContainer* container = BoxedContainer::createContainer(defaultContainerPath, "Default", GlobalSettings::wineVersions[0].name);
        std::string s = Fs::nativePathSeperator;
        std::string nativePath = GlobalSettings::getRootFolder(container) + s + "home" + s + "username" + s + ".wine" + s + "drive_c" + s + "windows" + s + "system32";
        Fs::makeNativeDirs(nativePath);
        // so that icon detection works
        FsZip::extractFileFromZip(wineVersions[0].filePath, "home/username/.wine/drive_c/windows/system32/winemine.exe", nativePath);
        BoxedApp app("WineMine", "/home/username/.wine/drive_c/windows/system32", "winemine.exe", container);
        app.saveApp();
    }
    GlobalSettings::loadFileList();
    GlobalSettings::checkFileListForUpdate();
}

void GlobalSettings::saveConfig() {
    ConfigFile config(GlobalSettings::configFilePath);
    config.writeString("Version", "1");
    config.writeString("DataFolder", GlobalSettings::dataFolderLocation);
    config.writeString("Theme", GlobalSettings::theme);
    config.writeString("DefaultResolution", GlobalSettings::defaultResolution);
    config.writeInt("DefaultScale", GlobalSettings::defaultScale);
    config.writeInt("FontScale", (int)(GlobalSettings::fontScale*100));
    config.writeString("FilesURL", GlobalSettings::filesUrl);
    config.saveChanges();
}

std::string GlobalSettings::getFileFromWineName(const std::string& name) {
    for (auto& ver : GlobalSettings::wineVersions) {
        if (stringCaseInSensativeEquals(ver.name, name)) {
            return ver.filePath;
        }
    }
    return "";
}

void GlobalSettings::lookForFileSystems(const std::string& path) {
    Fs::iterateAllNativeFiles(path, true, false, [] (const std::string& filepath, bool isDir)->U32 {
        if (stringHasEnding(filepath, ".zip", true)) {
            std::string wineVersion;
            if (FsZip::readFileFromZip(filepath, "wineVersion.txt", wineVersion) && wineVersion.length() && !GlobalSettings::getFileFromWineName(wineVersion).length()) {
                std::string fsVersion;
                FsZip::readFileFromZip(filepath, "version.txt", fsVersion);
                GlobalSettings::wineVersions.push_back(WineVersion(wineVersion, fsVersion, filepath));
            }
        }
        return 0;
    });
}

void GlobalSettings::initWineVersions() {
    GlobalSettings::lookForFileSystems(GlobalSettings::getFileSystemFolder());
    GlobalSettings::lookForFileSystems(GlobalSettings::exePath);
    GlobalSettings::lookForFileSystems(GlobalSettings::exePath + Fs::nativePathSeperator + "FileSystems");
    GlobalSettings::lookForFileSystems(GlobalSettings::exePath + Fs::nativePathSeperator + ".." + Fs::nativePathSeperator + "FileSystems");
    std::sort(GlobalSettings::wineVersions.rbegin(), GlobalSettings::wineVersions.rend());
}

void GlobalSettings::reloadWineVersions() {
    GlobalSettings::wineVersions.clear();
    GlobalSettings::lookForFileSystems(GlobalSettings::getFileSystemFolder());
    GlobalSettings::lookForFileSystems(GlobalSettings::exePath);
    GlobalSettings::lookForFileSystems(GlobalSettings::exePath + Fs::nativePathSeperator + "FileSystems");
    GlobalSettings::lookForFileSystems(GlobalSettings::exePath + Fs::nativePathSeperator + ".." + Fs::nativePathSeperator + "FileSystems");
    std::sort(GlobalSettings::wineVersions.rbegin(), GlobalSettings::wineVersions.rend());
}

std::string GlobalSettings::getContainerFolder() {
    return GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "Containers";
}

std::string GlobalSettings::getFileSystemFolder() {
    return GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "FileSystems";
}

std::string GlobalSettings::getDemoFolder() {
    return GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "DemoCache";
}

std::string GlobalSettings::getRootFolder(BoxedContainer* container) {
    return container->dirPath + Fs::nativePathSeperator + "root";
}

std::string GlobalSettings::getAppFolder(BoxedContainer* container) {
    return container->dirPath + Fs::nativePathSeperator + "apps";
}

U32 GlobalSettings::scaleIntUI(U32 value) {
    return value * scale / SCALE_DENOMINATOR;
}

float GlobalSettings::scaleFloatUI(float value) {
    return value * scale / SCALE_DENOMINATOR;
}

float GlobalSettings::scaleFloatUIAndFont(float value) {
    return value * scale / SCALE_DENOMINATOR * fontScale;
}

void GlobalSettings::setScale(U32 scale) {
    GlobalSettings::scale = scale;
    UiSettings::ICON_SIZE = GlobalSettings::scaleIntUI(48);
    GlobalSettings::extraVerticalSpacing = (float)GlobalSettings::scaleIntUI(5);
}

void GlobalSettings::loadTheme() {
    if (GlobalSettings::theme == "Classic") {        
        ImGui::StyleColorsClassic();
        ImGui::GetStyle().FrameBorderSize = 0.0f;
    } else if (GlobalSettings::theme == "Light") {
        ImGui::StyleColorsLight();
        ImGui::GetStyle().FrameBorderSize = 1.0f;
    } else {
        ImGui::StyleColorsDark();        
        ImGui::GetStyle().FrameBorderSize = 0.0f;
    }
}

void loadApps();
void GlobalSettings::reloadApps() {
    loadApps();
}

void GlobalSettings::loadFileList() {
    BOXEDWINE_CRITICAL_SECTION;

    std::string filesConfigPath = GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "files.xml";
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filesConfigPath.c_str());
    if (!result) {
        return;
    }
    pugi::xml_node node = doc.child("XML");
    GlobalSettings::availableWineVersions.clear();
    for (pugi::xml_node wine : node.children("Wine")) {
        std::string name = wine.child("Name").text().as_string();
        std::string ver = wine.child("FileVersion").text().as_string();
        std::string file = wine.child("FileURL").text().as_string();
        int fileSize = wine.child("FileSizeMB").text().as_int();

        if (name.length() && ver.length() && file.length()) {
            GlobalSettings::availableWineVersions.push_back(WineVersion(name, ver, file, fileSize));
        } else {
            break;
        }
    }
    GlobalSettings::demos.clear();
    for (pugi::xml_node demo : node.children("Demo")) {
        std::string name = demo.child("Name").text().as_string();
        std::string installType = demo.child("InstallType").text().as_string();
        std::string icon = demo.child("IconURL").text().as_string();
        int fileSize = demo.child("FileSizeMB").text().as_int();
        std::string file = demo.child("FileURL").text().as_string();
        std::string exe = demo.child("ShortcutExe").text().as_string();
        std::string installExe = demo.child("InstallExe").text().as_string();
        std::string help = demo.child("Help").text().as_string();
        std::string options = demo.child("Options").text().as_string();
        std::string installOptions = demo.child("InstallOptions").text().as_string();
        if (name.length() && file.length()) {
            GlobalSettings::demos.push_back(AppFile(name, installType, icon, file, fileSize, exe, options, help, "", installOptions, installExe));
        } else {
            break;
        }
    }
    GlobalSettings::components.clear();
    for (pugi::xml_node component : node.children("Component")) {
        std::string name = component.child("Name").text().as_string();
        std::string optionsName = component.child("OptionsName").text().as_string();
        std::string installType = component.child("InstallType").text().as_string();
        std::string icon = component.child("IconURL").text().as_string();
        int fileSize = component.child("FileSizeMB").text().as_int();
        std::string file = component.child("FileURL").text().as_string();
        std::string exe = component.child("ShortcutExe").text().as_string();
        std::string help = component.child("Help").text().as_string();
        std::string options = component.child("Options").text().as_string();
        std::string installOptions = component.child("InstallOptions").text().as_string();
        if (name.length() && file.length()) {
            GlobalSettings::components.push_back(AppFile(name, installType, icon, file, fileSize, exe, options, help, optionsName, installOptions, ""));
        } else {
            break;
        }
    }
    runOnMainUI([]() { // might not have an OpenGL context while starting up
        for (auto& demo : GlobalSettings::getDemos()) {
            demo.buildIconTexture();
        }
        for (auto& component : GlobalSettings::getComponents()) {
            component.buildIconTexture();
        }
        return false;
        });
}

AppFile* GlobalSettings::getComponentByOptionName(const std::string& name) {
    for (auto& app : GlobalSettings::components) {
        if (app.optionsName == name) {
            return &app;
        }
    }
    return NULL;
}

bool GlobalSettings::checkFileListForUpdate() {
    std::string filesConfigPath = GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "files.xml";

    PLATFORM_STAT_STRUCT buf;
    if (PLATFORM_STAT(filesConfigPath.c_str(), &buf) == 0) {
        U64 t = (U64)buf.st_mtime; // time as seconds
        U64 currentTime = KSystem::getSystemTimeAsMicroSeconds() / 1000000l;
        if (t + 24 * 60 * 60 < currentTime) {
            updateFileList(filesConfigPath);
            return true;
        }
    } else {
        updateFileList(filesConfigPath);
        return true;
    }    
    return false;
}

bool GlobalSettings::isFilesListDownloading() {
    return GlobalSettings::filesListDownloading;
}

void GlobalSettings::updateFileList(const std::string& fileLocation) {
    runInBackgroundThread([fileLocation]() {
        std::string errorMsg;
        GlobalSettings::filesListDownloading = true;
        ::downloadFile(GlobalSettings::filesUrl, fileLocation, [](U64 bytesCompleted) {
            }, NULL, errorMsg);
        runOnMainUI([]() {
            GlobalSettings::loadFileList();
            GlobalSettings::filesListDownloading = false;
            runInBackgroundThread([]() {
                std::string errorMsg;

                for (auto& demo : GlobalSettings::getDemos()) {
                    if (demo.iconPath.length()) {
                        size_t pos = demo.iconPath.rfind("/");
                        if (pos == std::string::npos) {
                            return; // :TODO: error msg?
                        }
                        if (!Fs::doesNativePathExist(GlobalSettings::getDemoFolder())) {
                            Fs::makeNativeDirs(GlobalSettings::getDemoFolder());
                        }
                        if (!Fs::doesNativePathExist(demo.localIconPath)) {
                            ::downloadFile(demo.iconPath, demo.localIconPath, [](U64 bytesCompleted) {
                                }, NULL, errorMsg);
                            runOnMainUI([&demo]() {
                                demo.buildIconTexture();
                                return false;
                                });
                        }
                    }
                }
            });
            return false;
        });
    }       
    );
}

void GlobalSettings::loadFonts() {
    std::string fontsPath = GlobalSettings::dataFolderLocation + Fs::nativePathSeperator + "Fonts";
    if (!Fs::doesNativePathExist(fontsPath)) {
        Fs::makeNativeDirs(fontsPath);
    }
    std::string sansBoldFontsPath = fontsPath + Fs::nativePathSeperator + "LiberationSans-Bold.ttf";
    if (!Fs::doesNativePathExist(sansBoldFontsPath) && wineVersions.size() > 0) {
        FsZip::extractFileFromZip(wineVersions[0].filePath, "usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", fontsPath);
    }
    std::string sansFontsPath = fontsPath + Fs::nativePathSeperator + "LiberationSans-Regular.ttf";
    if (!Fs::doesNativePathExist(sansFontsPath) && wineVersions.size() > 0) {
        FsZip::extractFileFromZip(wineVersions[0].filePath, "usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", fontsPath);
    }
    std::string awesomeFontPath = fontsPath + Fs::nativePathSeperator + "fontawesome-webfont.ttf";
    if (!Fs::doesNativePathExist(awesomeFontPath) && wineVersions.size() > 0) {
        FsZip::extractFileFromZip(wineVersions[0].filePath, "usr/share/fonts/truetype/fontawesome-webfont.ttf", fontsPath);
    }

    ImGuiIO& io = ImGui::GetIO();
    // first font added will be default
    if (Fs::doesNativePathExist(sansFontsPath) && !Fs::isNativePathDirectory(sansFontsPath)) {
        defaultFont = io.Fonts->AddFontFromFileTTF(sansFontsPath.c_str(), floor(scaleFloatUI(15.0f * GlobalSettings::fontScale)));
        if (Fs::doesNativePathExist(awesomeFontPath)) {
            static const ImWchar icons_ranges[] = { 0xf000, 0xf2b4, 0 }; // will not be copied by AddFont* so keep in scope.
            ImFontConfig config;
            config.MergeMode = true;
            if (io.Fonts->AddFontFromFileTTF(awesomeFontPath.c_str(), floor(scaleFloatUI(15.0f * GlobalSettings::fontScale)), &config, icons_ranges)) {
                GlobalSettings::iconFontsLoaded = true;
            }
        }
        mediumFont = io.Fonts->AddFontFromFileTTF(sansFontsPath.c_str(), floor(scaleFloatUI(20.0f * GlobalSettings::fontScale)));
        if (Fs::doesNativePathExist(awesomeFontPath)) {
            static const ImWchar icons_ranges[] = { 0xf000, 0xf2b4, 0 }; // will not be copied by AddFont* so keep in scope.
            ImFontConfig config;
            config.MergeMode = true;
            if (io.Fonts->AddFontFromFileTTF(awesomeFontPath.c_str(), floor(scaleFloatUI(20.0f * GlobalSettings::fontScale)), &config, icons_ranges)) {
                GlobalSettings::iconFontsLoaded = true;
            }
        }
        largeFont = io.Fonts->AddFontFromFileTTF(sansFontsPath.c_str(), floor(scaleFloatUI(25.0f * GlobalSettings::fontScale)));
        if (Fs::doesNativePathExist(awesomeFontPath)) {
            static const ImWchar icons_ranges[] = { 0xf000, 0xf2b4, 0 }; // will not be copied by AddFont* so keep in scope.
            ImFontConfig config;
            config.MergeMode = true;
            if (io.Fonts->AddFontFromFileTTF(awesomeFontPath.c_str(), floor(scaleFloatUI(25.0f * GlobalSettings::fontScale)), &config, icons_ranges)) {
                GlobalSettings::iconFontsLoaded = true;
            }
        }
    }

    if (Fs::doesNativePathExist(sansBoldFontsPath) && !Fs::isNativePathDirectory(sansBoldFontsPath)) {
        largeFontBold = io.Fonts->AddFontFromFileTTF(sansBoldFontsPath.c_str(), scaleFloatUI(24.0f * GlobalSettings::fontScale));
        if (Fs::doesNativePathExist(awesomeFontPath)) {
            static const ImWchar icons_ranges[] = { 0xf000, 0xf2b4, 0 }; // will not be copied by AddFont* so keep in scope.
            ImFontConfig config;
            config.MergeMode = true;
            if (io.Fonts->AddFontFromFileTTF(awesomeFontPath.c_str(), floor(scaleFloatUI(25.0f * GlobalSettings::fontScale)), &config, icons_ranges)) {
                GlobalSettings::iconFontsLoaded = true;
            }
        }
    }    
}

void GlobalSettings::downloadFile(const std::string& url, const std::string& filePath, const std::string& name, U32 sizeMB, std::function<void(bool)> onCompleted) {
    runOnMainUI([url, filePath, name, sizeMB, onCompleted]() {
        std::string parentPath = Fs::getNativeParentPath(filePath);
        if (!Fs::doesNativePathExist(parentPath)) {
            Fs::makeNativeDirs(parentPath);
        }
        new DownloadDlg(DOWNLOADDLG_TITLE, getTranslationWithFormat(DOWNLOADDLG_LABEL, true, name), url, filePath, [onCompleted](bool success) {
            runOnMainUI([success, onCompleted]() {
                GlobalSettings::reloadWineVersions();
                if (!GlobalSettings::defaultFont) {
                    GlobalSettings::restartUI = true;
                }
                onCompleted(success);
                return false;
                });
            }, ((U64)sizeMB) * 1024 * 1024);
        return false;
        });
}

void GlobalSettings::downloadWine(const WineVersion& version, std::function<void(bool)> onCompleted) {
    runOnMainUI([&version, onCompleted]() {
        size_t pos = version.filePath.rfind("/");
        if (pos == std::string::npos) {
            return false; // :TODO: error msg?
        }
        std::string fileName = version.filePath.substr(pos + 1);
        std::string filePath = GlobalSettings::getFileSystemFolder() + Fs::nativePathSeperator + fileName;
        if (!Fs::doesNativePathExist(GlobalSettings::getFileSystemFolder())) {
            Fs::makeNativeDirs(GlobalSettings::getFileSystemFolder());
        }
        new DownloadDlg(DOWNLOADDLG_TITLE, getTranslationWithFormat(DOWNLOADDLG_LABEL, true, version.name), version.filePath, filePath, [onCompleted](bool success) {
            runOnMainUI([success, onCompleted]() {
                GlobalSettings::reloadWineVersions();
                if (!GlobalSettings::defaultFont) {
                    GlobalSettings::restartUI = true;
                }
                onCompleted(success);
                return false;
                });
            }, ((U64)(version.size))*1024*1024);
        return false;
        });
}

void GlobalSettings::useFastFrameRate(bool useFast) {
    if (useFast) {
        if (GlobalSettings::fastFrameRateCount == 0) {
            GlobalSettings::frameDelayMillies = FAST_FRAME_DELAY;
        }
        GlobalSettings::fastFrameRateCount++;
    } else {
        GlobalSettings::fastFrameRateCount--;
        if (GlobalSettings::fastFrameRateCount == 0) {
            GlobalSettings::frameDelayMillies = SLOW_FRAME_DELAY;
        }
    }
    GlobalSettings::lastFrameDelayChange = KSystem::getSystemTimeAsMicroSeconds();
}

void GlobalSettings::updateLastFrameDelayChange() {
    GlobalSettings::lastFrameDelayChange = KSystem::getSystemTimeAsMicroSeconds();
}

U32 GlobalSettings::getFrameDelayMillies() { 
    if (lastFrameDelayChange + 1000000l < KSystem::getSystemTimeAsMicroSeconds()) {
        return GlobalSettings::frameDelayMillies;
    }
    return FAST_FRAME_DELAY;
}

void GlobalSettings::setTheme(const std::string& theme) { 
    GlobalSettings::theme = theme; 
    loadTheme();
}

void GlobalSettings::setFontScale(float scale) {
    GlobalSettings::fontScale = scale;
}

std::string GlobalSettings::createUniqueContainerPath(const std::string& name) {
    std::string containerName = name;
    stringReplaceAll(containerName, " ", "_");
    stringReplaceAll(containerName, ":", "");

    std::srand((U32)std::time(nullptr));
    while (true) {
        int r = std::rand();
        char tmp[10];
        SDL_itoa(r, tmp, 16);
        std::string result = GlobalSettings::getContainerFolder() + Fs::nativePathSeperator + containerName + "-"+tmp;
        if (!Fs::doesNativePathExist(result)) {
            return result;
        }
    }
}
