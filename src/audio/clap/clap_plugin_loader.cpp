#include "eatsbits/audio/clap/clap_plugin_loader.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <thread>
#include <unordered_set>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace eatsbits::audio {

// ============================================================================
// ClapLibraryHandle Implementation
// ============================================================================

ClapLibraryHandle::ClapLibraryHandle(
    std::string path,
    void* osHandle,
    const clap_plugin_entry_t* entry,
    const clap_plugin_factory_t* factory
) : path_(std::move(path)),
    osHandle_(osHandle),
    entry_(entry),
    factory_(factory) {}

static const clap_host_t sProbeHost{
    CLAP_VERSION,
    nullptr,
    "Eatsbits Probe",
    "Maddest Labs",
    "https://github.com/maddestlabs/eatsbits",
    "1.0.0",
    [](const clap_host*, const char*) -> const void* { return nullptr; },
    [](const clap_host*) {},
    [](const clap_host*) {},
    [](const clap_host*) {}
};

ClapLibraryHandle::~ClapLibraryHandle() {
    if (entry_ && entry_->deinit) {
        entry_->deinit();
    }
    if (osHandle_) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(osHandle_));
#elif !defined(__EMSCRIPTEN__)
        dlclose(osHandle_);
#endif
        osHandle_ = nullptr;
    }
    entry_ = nullptr;
    factory_ = nullptr;
}

std::shared_ptr<ClapLibraryHandle> ClapLibraryHandle::load(const std::string& path) {
#if defined(__EMSCRIPTEN__)
    (void)path;
    return nullptr;
#else
    void* handle = nullptr;

#if defined(_WIN32)
    HMODULE hLib = LoadLibraryExA(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hLib) {
        hLib = LoadLibraryA(path.c_str());
    }
    if (!hLib) {
        return nullptr;
    }
    handle = static_cast<void*>(hLib);
    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(hLib, "clap_entry"));
#else
    void* hLib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!hLib) {
        return nullptr;
    }
    handle = hLib;
    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(dlsym(hLib, "clap_entry"));
#endif

    if (!entry) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return nullptr;
    }

    if (!clap_version_is_compatible(entry->clap_version)) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return nullptr;
    }

    if (!entry->init || !entry->init(path.c_str())) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return nullptr;
    }

    auto* factory = static_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory) {
        entry->deinit();
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return nullptr;
    }

    return std::shared_ptr<ClapLibraryHandle>(new ClapLibraryHandle(path, handle, entry, factory));
#endif
}

const clap_plugin_t* ClapLibraryHandle::createPlugin(const clap_host_t* host, const std::string& pluginId) {
    if (!factory_ || !factory_->create_plugin) {
        return nullptr;
    }
    return factory_->create_plugin(factory_, host, pluginId.c_str());
}

// ============================================================================
// ClapPluginLoader Implementation
// ============================================================================

ClapPluginLoader& ClapPluginLoader::instance() {
    static ClapPluginLoader sInstance;
    return sInstance;
}

static std::filesystem::path getExecutableDirectory() {
    std::error_code ec;
#if defined(_WIN32)
    char buffer[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (len > 0) {
        return std::filesystem::path(buffer).parent_path();
    }
#elif defined(__APPLE__)
    char buffer[1024] = {0};
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::filesystem::path(buffer).parent_path();
    }
#elif defined(__linux__)
    char buffer[1024] = {0};
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path(ec);
}

std::vector<std::filesystem::path> ClapPluginLoader::getDefaultSearchPaths() const {
    std::vector<std::filesystem::path> paths;

#if defined(_WIN32)
    // 1. %CommonProgramFiles%\CLAP (typically C:\Program Files\Common Files\CLAP)
    char commonProgFiles[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableA("CommonProgramFiles", commonProgFiles, MAX_PATH);
    if (len > 0) {
        paths.push_back(std::filesystem::path(commonProgFiles) / "CLAP");
    }

    // 2. %LOCALAPPDATA%\Programs\Common\CLAP
    char localAppData[MAX_PATH] = {0};
    len = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (len > 0) {
        paths.push_back(std::filesystem::path(localAppData) / "Programs" / "Common" / "CLAP");
    }
#elif defined(__APPLE__)
    // 1. /Library/Audio/Plug-Ins/CLAP
    paths.push_back("/Library/Audio/Plug-Ins/CLAP");

    // 2. ~/Library/Audio/Plug-Ins/CLAP
    const char* home = std::getenv("HOME");
    if (home) {
        paths.push_back(std::filesystem::path(home) / "Library/Audio/Plug-Ins/CLAP");
    }
#else
    // Linux standard paths
    // 1. ~/.clap
    const char* home = std::getenv("HOME");
    if (home) {
        paths.push_back(std::filesystem::path(home) / ".clap");
    }
    // 2. /usr/lib/clap & /usr/local/lib/clap
    paths.push_back("/usr/lib/clap");
    paths.push_back("/usr/local/lib/clap");
#endif

    // 3. Binary / Current Working Directory (Portable / Standalone app mode, similar to Reaper)
    if (includeBinaryPath_) {
        auto exeDir = getExecutableDirectory();
        std::error_code ec;
        if (!exeDir.empty()) {
            paths.push_back(exeDir / "plugins");
            paths.push_back(exeDir);
        }

        auto cwd = std::filesystem::current_path(ec);
        if (!cwd.empty() && cwd != exeDir) {
            paths.push_back(cwd / "plugins");
            paths.push_back(cwd);
        }
        if (std::filesystem::exists(cwd / "build" / "Release", ec)) {
            paths.push_back(cwd / "build" / "Release");
        }
        if (std::filesystem::exists(cwd / "build", ec)) {
            paths.push_back(cwd / "build");
        }
    }

    // 4. Custom user configured search paths
    for (const auto& cp : customSearchPaths_) {
        paths.push_back(cp);
    }

    // Deduplicate paths while preserving order
    std::vector<std::filesystem::path> uniquePaths;
    for (const auto& p : paths) {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec) || !std::filesystem::is_directory(p, ec)) {
            continue;
        }
        bool alreadyIncluded = false;
        for (const auto& u : uniquePaths) {
            std::error_code compEc;
            if (p == u || std::filesystem::equivalent(p, u, compEc)) {
                alreadyIncluded = true;
                break;
            }
        }
        if (!alreadyIncluded) {
            uniquePaths.push_back(p);
        }
    }

    return uniquePaths;
}

struct ScanningGuard {
    std::atomic<bool>& flag;
    explicit ScanningGuard(std::atomic<bool>& f) : flag(f) { flag.store(true); }
    ~ScanningGuard() { flag.store(false); }
};

std::vector<ClapPluginDescriptor> ClapPluginLoader::scanDirectories(
    const std::vector<std::filesystem::path>& customPaths
) {
    ScanningGuard guard(isScanning_);
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::filesystem::path> searchPaths = customPaths.empty()
        ? getDefaultSearchPaths()
        : customPaths;

    std::unordered_set<std::string> scannedFilesInPass;

    for (const auto& dirPath : searchPaths) {
        std::error_code ec;
        if (!std::filesystem::exists(dirPath, ec) || !std::filesystem::is_directory(dirPath, ec)) {
            continue;
        }

        auto dirIter = std::filesystem::recursive_directory_iterator(
            dirPath,
            std::filesystem::directory_options::skip_permission_denied,
            ec
        );

        for (auto it = dirIter; it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;

            // Never recurse into VCS, temporary or IDE internals
            if (it->is_directory(ec)) {
                std::string dirname = it->path().filename().string();
                if (dirname == ".git" || dirname == ".agents" || dirname == "node_modules" ||
                    dirname == "CMakeFiles" || dirname == ".vs" || dirname == ".vscode" ||
                    dirname == ".cache" || dirname == ".idea") {
                    it.disable_recursion_pending();
                    continue;
                }
            }

            std::error_code isFileEc;
            if (!it->is_regular_file(isFileEc)) {
                continue;
            }

            const auto& p = it->path();
            std::string ext = p.extension().string();
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (ext == ".clap" || ext == ".dll" || ext == ".so") {
                std::string fullPath = p.string();

                if (scannedFilesInPass.count(fullPath) > 0) {
                    continue;
                }
                scannedFilesInPass.insert(fullPath);

                // Skip files previously verified to NOT be CLAP plugins
                if (nonClapLibraries_.count(fullPath) > 0) {
                    continue;
                }

                // Check if already discovered
                bool alreadyDiscovered = false;
                for (const auto& disc : discoveredPlugins_) {
                    if (disc.libraryPath == fullPath) {
                        alreadyDiscovered = true;
                        break;
                    }
                }
                if (alreadyDiscovered) continue;

                // Attempt to load and inspect
                auto libHandle = ClapLibraryHandle::load(fullPath);
                if (!libHandle || !libHandle->isValid()) {
                    nonClapLibraries_.insert(fullPath);
                    continue;
                }

                const auto* factory = libHandle->getPluginFactory();
                if (!factory) {
                    nonClapLibraries_.insert(fullPath);
                    continue;
                }

                const uint32_t count = factory->get_plugin_count(factory);
                for (uint32_t i = 0; i < count; ++i) {
                    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, i);
                    if (!desc || !desc->id || !desc->name) continue;

                    ClapPluginDescriptor pluginDesc{};
                    pluginDesc.id = desc->id;
                    pluginDesc.name = desc->name;
                    pluginDesc.vendor = desc->vendor ? desc->vendor : "";
                    pluginDesc.version = desc->version ? desc->version : "";
                    pluginDesc.description = desc->description ? desc->description : "";
                    pluginDesc.url = desc->url ? desc->url : "";
                    pluginDesc.libraryPath = fullPath;
                    pluginDesc.pluginIndex = i;

                    if (desc->features) {
                        for (const char* const* feat = desc->features; *feat; ++feat) {
                            pluginDesc.features.emplace_back(*feat);
                            std::string fLower = *feat;
                            for (char& c : fLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                            if (fLower == "instrument" || fLower == "synthesizer" || fLower == "sampler" || fLower == "drum") {
                                pluginDesc.isInstrument = true;
                            } else if (fLower == "audio-effect" || fLower == "effect" || fLower == "filter" ||
                                       fLower == "distortion" || fLower == "delay" || fLower == "reverb") {
                                pluginDesc.isAudioEffect = true;
                            }
                        }
                    }

                    // Fallback classification if features array didn't specify
                    if (!pluginDesc.isInstrument && !pluginDesc.isAudioEffect) {
                        std::string nameLower = pluginDesc.name;
                        for (char& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (nameLower.find("synth") != std::string::npos || nameLower.find("303") != std::string::npos ||
                            nameLower.find("piano") != std::string::npos || nameLower.find("drum") != std::string::npos ||
                            nameLower.find("surge") != std::string::npos || nameLower.find("vital") != std::string::npos) {
                            pluginDesc.isInstrument = true;
                        } else {
                            pluginDesc.isAudioEffect = true;
                        }
                    }

                    // Check GUI support using valid probe host
                    const clap_plugin_t* probePlugin = factory->create_plugin(factory, &sProbeHost, desc->id);
                    if (probePlugin) {
                        if (probePlugin->init && probePlugin->init(probePlugin)) {
                            if (probePlugin->get_extension) {
                                auto* gui = probePlugin->get_extension(probePlugin, CLAP_EXT_GUI);
                                pluginDesc.hasGui = (gui != nullptr);
                            }
                        }
                        if (probePlugin->destroy) {
                            probePlugin->destroy(probePlugin);
                        }
                    }

                    discoveredPlugins_.push_back(std::move(pluginDesc));
                }
            }
        }
    }

    return discoveredPlugins_;
}

void ClapPluginLoader::scanDirectoriesAsync(
    const std::vector<std::filesystem::path>& customPaths,
    std::function<void(const std::vector<ClapPluginDescriptor>&)> onComplete
) {
    if (isScanning_.load()) {
        return; // Already actively scanning
    }
#if !defined(__EMSCRIPTEN__)
    std::thread([this, customPaths, onComplete = std::move(onComplete)]() {
        auto plugins = this->scanDirectories(customPaths);
        if (onComplete) {
            onComplete(plugins);
        }
    }).detach();
#else
    // On Web/WASM, dynamic desktop libraries cannot be probed via dlopen/LoadLibrary
    auto plugins = this->scanDirectories(customPaths);
    if (onComplete) {
        onComplete(plugins);
    }
#endif
}

std::vector<ClapPluginDescriptor> ClapPluginLoader::getDiscoveredPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return discoveredPlugins_;
}

void ClapPluginLoader::setCustomSearchPaths(const std::vector<std::filesystem::path>& paths) {
    std::lock_guard<std::mutex> lock(mutex_);
    customSearchPaths_ = paths;
}

void ClapPluginLoader::addCustomSearchPath(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    customSearchPaths_.push_back(path);
}

std::vector<std::filesystem::path> ClapPluginLoader::getCustomSearchPaths() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return customSearchPaths_;
}

const ClapPluginDescriptor* ClapPluginLoader::findPluginById(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& p : discoveredPlugins_) {
        if (p.id == pluginId) {
            return &p;
        }
    }
    return nullptr;
}

const ClapPluginDescriptor* ClapPluginLoader::findPluginByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& p : discoveredPlugins_) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

const clap_plugin_t* ClapPluginLoader::createPluginInstance(
    const std::string& libraryPath,
    const std::string& pluginId,
    const clap_host_t* host,
    std::shared_ptr<ClapLibraryHandle>& outHandle
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if library already loaded in cache
    auto it = loadedLibraries_.find(libraryPath);
    if (it != loadedLibraries_.end()) {
        outHandle = it->second.lock();
    }

    if (!outHandle) {
        outHandle = ClapLibraryHandle::load(libraryPath);
        if (!outHandle) {
            return nullptr;
        }
        loadedLibraries_[libraryPath] = outHandle;
    }

    return outHandle->createPlugin(host, pluginId);
}

void ClapPluginLoader::registerCustomDescriptor(const ClapPluginDescriptor& desc) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& existing : discoveredPlugins_) {
        if (existing.id == desc.id) {
            existing = desc;
            return;
        }
    }
    discoveredPlugins_.push_back(desc);
}

void ClapPluginLoader::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    discoveredPlugins_.clear();
    loadedLibraries_.clear();
    nonClapLibraries_.clear();
}

} // namespace eatsbits::audio
