#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <atomic>
#include <functional>
#include "clap/clap.h"

namespace eatsbits::audio {

struct ClapPluginDescriptor {
    std::string id;
    std::string name;
    std::string vendor;
    std::string version;
    std::string description;
    std::string url;
    std::vector<std::string> features;
    std::string libraryPath;
    uint32_t pluginIndex{0};
    bool isInstrument{false};
    bool isAudioEffect{false};
    bool hasGui{false};
};

/**
 * RAII handle managing a loaded dynamic CLAP library (.clap / .dll / .so).
 * Ensures entry->init() is called once on load, and entry->deinit() and
 * library release occur safely on final reference destruction.
 */
class ClapLibraryHandle {
public:
    static std::shared_ptr<ClapLibraryHandle> load(const std::string& path);

    ~ClapLibraryHandle();

    // Prevent copying, allow moving
    ClapLibraryHandle(const ClapLibraryHandle&) = delete;
    ClapLibraryHandle& operator=(const ClapLibraryHandle&) = delete;

    [[nodiscard]] bool isValid() const noexcept { return entry_ != nullptr; }
    [[nodiscard]] const std::string& getPath() const noexcept { return path_; }
    [[nodiscard]] const clap_plugin_entry_t* getEntry() const noexcept { return entry_; }
    [[nodiscard]] const clap_plugin_factory_t* getPluginFactory() const noexcept { return factory_; }

    const clap_plugin_t* createPlugin(const clap_host_t* host, const std::string& pluginId);

private:
    explicit ClapLibraryHandle(std::string path, void* osHandle, const clap_plugin_entry_t* entry, const clap_plugin_factory_t* factory);

    std::string path_;
    void* osHandle_{nullptr};
    const clap_plugin_entry_t* entry_{nullptr};
    const clap_plugin_factory_t* factory_{nullptr};
};

/**
 * Singleton scanner and loader for system-wide and workspace CLAP plugins.
 * Scans standard OS locations (%CommonProgramFiles%\CLAP, ~/.clap, etc.),
 * inspects plugin descriptors, and instantiates plugins.
 */
class ClapPluginLoader {
public:
    static ClapPluginLoader& instance();

    // Standard search directories for the current OS platform
    [[nodiscard]] std::vector<std::filesystem::path> getDefaultSearchPaths() const;

    // Scan directories (default or custom) for all available .clap files and cache descriptors
    std::vector<ClapPluginDescriptor> scanDirectories(const std::vector<std::filesystem::path>& customPaths = {});

    // Asynchronous scan in a background thread to prevent UI stalls
    void scanDirectoriesAsync(
        const std::vector<std::filesystem::path>& customPaths = {},
        std::function<void(const std::vector<ClapPluginDescriptor>&)> onComplete = nullptr
    );

    [[nodiscard]] bool isScanning() const noexcept { return isScanning_.load(); }

    // Retrieve cached plugin descriptors from previous scan (thread-safe)
    [[nodiscard]] std::vector<ClapPluginDescriptor> getDiscoveredPlugins() const;

    // Configurable option to include current / binary directory (Portable / Standalone app mode, similar to Reaper)
    void setIncludeBinaryPath(bool enable) noexcept { includeBinaryPath_ = enable; }
    [[nodiscard]] bool isIncludeBinaryPathEnabled() const noexcept { return includeBinaryPath_; }

    // Custom search paths
    void setCustomSearchPaths(const std::vector<std::filesystem::path>& paths);
    void addCustomSearchPath(const std::filesystem::path& path);
    [[nodiscard]] std::vector<std::filesystem::path> getCustomSearchPaths() const;

    // Find descriptor by ID or name
    [[nodiscard]] const ClapPluginDescriptor* findPluginById(const std::string& pluginId) const;
    [[nodiscard]] const ClapPluginDescriptor* findPluginByName(const std::string& name) const;

    // Create an instance of a plugin
    const clap_plugin_t* createPluginInstance(
        const std::string& libraryPath,
        const std::string& pluginId,
        const clap_host_t* host,
        std::shared_ptr<ClapLibraryHandle>& outHandle
    );

    // Register an in-memory or custom plugin descriptor directly (e.g. for testing)
    void registerCustomDescriptor(const ClapPluginDescriptor& desc);

    // Clear scanned cache
    void clearCache();

private:
    ClapPluginLoader() = default;
    ~ClapPluginLoader() = default;

    mutable std::mutex mutex_;
    std::atomic<bool> isScanning_{false};
    bool includeBinaryPath_{true};
    std::vector<std::filesystem::path> customSearchPaths_;
    std::unordered_set<std::string> nonClapLibraries_;
    std::vector<ClapPluginDescriptor> discoveredPlugins_;
    std::unordered_map<std::string, std::weak_ptr<ClapLibraryHandle>> loadedLibraries_;
};

} // namespace eatsbits::audio
