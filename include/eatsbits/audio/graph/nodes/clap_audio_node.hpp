#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include "eatsbits/audio/graph/graph_node.hpp"
#include "eatsbits/audio/clap/clap_plugin_loader.hpp"
#include "clap/clap.h"

namespace eatsbits::audio {

struct ClapParamDef {
    clap_id id{0};
    std::string name;
    std::string module;
    double minValue{0.0};
    double maxValue{1.0};
    double defaultValue{0.5};
    double currentValue{0.5};
    uint32_t flags{0};
};

/**
 * Modular Audio Graph Node hosting a 3rd-party CLAP Instrument or Audio Effect.
 * Executes native plugin DSP with sample-accurate parameter and note event dispatch,
 * zero heap allocations during processBlock(), full state save/load,
 * Win32 floating window native GUI, and generic inspector parameter fallback.
 */
class ClapAudioNode : public GraphNode {
public:
    // Create by loading from .clap library path and plugin ID
    static std::shared_ptr<ClapAudioNode> create(
        const std::string& libraryPath,
        const std::string& pluginId,
        std::string nodeName = ""
    );

    explicit ClapAudioNode(std::string name = "ClapPlugin");

    // Create directly with an existing plugin instance and library handle
    ClapAudioNode(
        const clap_plugin_t* plugin,
        std::shared_ptr<ClapLibraryHandle> libHandle,
        std::string name = "ClapPlugin"
    );

    ~ClapAudioNode() override;

    bool initFromLibrary(const std::string& libraryPath, const std::string& pluginId);
    bool attachPlugin(const clap_plugin_t* plugin, std::shared_ptr<ClapLibraryHandle> libHandle);

    // GraphNode Lifecycle
    void prepare(double sampleRate, uint32_t maxBlockSize) override;
    void reset() noexcept override;
    void processBlock(uint32_t numFrames) noexcept override;

    // Real-Time Note and Parameter Event Handling
    void handleEvent(const AudioEvent& event) noexcept override;
    void noteOn(uint8_t note, float velocity, int16_t channel = 0) noexcept;
    void noteOff(uint8_t note, int16_t channel = 0) noexcept;
    void allNotesOff() noexcept;

    // Transport & Tempo
    void setBpm(double bpm) noexcept { bpm_ = bpm; }
    [[nodiscard]] double getBpm() const noexcept { return bpm_; }
    void setTransportPlaying(bool playing) noexcept { isPlaying_ = playing; }
    [[nodiscard]] bool isTransportPlaying() const noexcept { return isPlaying_; }

    // Parameters (CLAP_EXT_PARAMS)
    void setParameter(uint32_t paramId, float value) noexcept override;
    void setParamValue(clap_id paramId, double value) noexcept;
    [[nodiscard]] double getParamValue(clap_id paramId) const noexcept;
    [[nodiscard]] uint32_t getParamCount() const noexcept;
    [[nodiscard]] std::vector<ClapParamDef> getParameterList() const;
    void reflectParameters();

    // State Persistence (CLAP_EXT_STATE)
    [[nodiscard]] std::vector<uint8_t> saveState() const;
    bool loadState(const std::vector<uint8_t>& stateData);

    // GUI Integration (CLAP_EXT_GUI)
    [[nodiscard]] bool hasGui() const noexcept;
    [[nodiscard]] bool isGuiOpen() const noexcept { return isGuiOpen_; }
    bool openGuiWindow();
    void closeGuiWindow();

    // Embedded GUI Integration (Docked child HWND)
    bool openEmbeddedGui(void* parentHwnd, int x, int y, int w, int h);
    void setEmbeddedGuiBounds(int x, int y, int w, int h);
    void showEmbeddedGui();
    void hideEmbeddedGui();
    void destroyEmbeddedGui();
    [[nodiscard]] bool isEmbeddedGuiOpen() const noexcept { return isEmbeddedGuiOpen_; }
    [[nodiscard]] void* getEmbeddedContainerHwnd() const noexcept { return embeddedContainerHwnd_; }
    [[nodiscard]] void* getGuiHwnd() const noexcept { return guiHwnd_; }
    [[nodiscard]] const clap_plugin_gui_t* getGuiExtension() const noexcept { return extGui_; }
    [[nodiscard]] static bool isAnyGuiOpen();

    std::function<void()> onGuiClosed;

    // Main Thread Callbacks & Verification
    static void setMainThreadId(std::thread::id id) noexcept;
    [[nodiscard]] static std::thread::id getMainThreadId() noexcept;
    static void pollAllMainThreadCallbacks();
    void pollMainThreadCallbacks();

    // Plugin Introspection
    [[nodiscard]] const clap_plugin_descriptor_t* getDescriptor() const noexcept {
        return plugin_ ? plugin_->desc : nullptr;
    }
    [[nodiscard]] std::string getPluginName() const {
        if (plugin_ && plugin_->desc && plugin_->desc->name) {
            return std::string(plugin_->desc->name);
        }
        return getName();
    }
    [[nodiscard]] const std::string& getPluginId() const noexcept { return pluginId_; }
    [[nodiscard]] bool isInstrument() const noexcept { return isInstrument_; }
    [[nodiscard]] bool isAudioEffect() const noexcept { return isAudioEffect_; }
    [[nodiscard]] const clap_plugin_t* getClapPlugin() const noexcept { return plugin_; }
    [[nodiscard]] bool isActivated() const noexcept { return isActivated_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isProcessing() const noexcept { return isProcessing_.load(std::memory_order_relaxed); }

private:
    void initHostStructure();
    void queryPluginConfiguration();

    // CLAP Host Callbacks
    static const void* CLAP_ABI hostGetExtension(const clap_host_t* host, const char* extension_id);
    static void CLAP_ABI hostRequestRestart(const clap_host_t* host);
    static void CLAP_ABI hostRequestProcess(const clap_host_t* host);
    static void CLAP_ABI hostRequestCallback(const clap_host_t* host);

    // Host Extensions
    static bool CLAP_ABI hostThreadCheckIsMainThread(const clap_host_t* host);
    static bool CLAP_ABI hostThreadCheckIsAudioThread(const clap_host_t* host);

    static void CLAP_ABI hostParamsRescan(const clap_host_t* host, clap_param_rescan_flags flags);
    static void CLAP_ABI hostParamsClear(const clap_host_t* host, clap_id param_id, clap_param_clear_flags flags);
    static void CLAP_ABI hostParamsRequestFlush(const clap_host_t* host);

    static void CLAP_ABI hostLatencyChanged(const clap_host_t* host);
    static void CLAP_ABI hostStateMarkDirty(const clap_host_t* host);
    static void CLAP_ABI hostLog(const clap_host_t* host, clap_log_severity severity, const char* msg);

    static bool CLAP_ABI hostAudioPortsIsRescanFlagSupported(const clap_host_t* host, uint32_t flag);
    static void CLAP_ABI hostAudioPortsRescan(const clap_host_t* host, uint32_t flags);

    static uint32_t CLAP_ABI hostNotePortsSupportedDialects(const clap_host_t* host);
    static void CLAP_ABI hostNotePortsRescan(const clap_host_t* host, uint32_t flags);

    static void CLAP_ABI hostTailChanged(const clap_host_t* host);
    static bool CLAP_ABI hostTimerRegister(const clap_host_t* host, uint32_t period_ms, clap_id* timer_id);
    static bool CLAP_ABI hostTimerUnregister(const clap_host_t* host, clap_id timer_id);

    static void CLAP_ABI hostGuiResizeHintsChanged(const clap_host_t* host);
    static bool CLAP_ABI hostGuiRequestResize(const clap_host_t* host, uint32_t width, uint32_t height);
    static bool CLAP_ABI hostGuiRequestShow(const clap_host_t* host);
    static bool CLAP_ABI hostGuiRequestHide(const clap_host_t* host);
    static void CLAP_ABI hostGuiClosed(const clap_host_t* host, bool was_destroyed);

    // Real-Time Event List Callbacks
    static uint32_t CLAP_ABI inEventsSize(const clap_input_events_t* list);
    static const clap_event_header_t* CLAP_ABI inEventsGet(const clap_input_events_t* list, uint32_t index);
    static bool CLAP_ABI outEventsTryPush(const clap_output_events_t* list, const clap_event_header_t* event);

    const clap_plugin_t* plugin_{nullptr};
    std::shared_ptr<ClapLibraryHandle> libHandle_;
    std::string pluginId_;
    bool isInstrument_{false};
    bool isAudioEffect_{true};

    // Thread verification
#if !defined(__EMSCRIPTEN__)
    std::thread::id mainThreadId_{};
    std::atomic<std::thread::id> audioThreadId_{};
#endif

    // Transport & Timing
    double bpm_{120.0};
    bool isPlaying_{false};
    clap_event_transport_t transport_{};

    // Host structure & extensions provided to plugins
    clap_host_t host_{};
    clap_host_gui_t hostGui_{};
    clap_host_thread_check_t hostThreadCheck_{};
    clap_host_params_t hostParams_{};
    clap_host_latency_t hostLatency_{};
    clap_host_state_t hostState_{};
    clap_host_log_t hostLog_{};
    clap_host_audio_ports_t hostAudioPorts_{};
    clap_host_note_ports_t hostNotePorts_{};
    clap_host_tail_t hostTail_{};
    clap_host_timer_support_t hostTimerSupport_{};

    // Extensions cached from plugin
    const clap_plugin_params_t* extParams_{nullptr};
    const clap_plugin_state_t* extState_{nullptr};
    const clap_plugin_gui_t* extGui_{nullptr};
    const clap_plugin_audio_ports_t* extAudioPorts_{nullptr};
    const clap_plugin_note_ports_t* extNotePorts_{nullptr};
    const clap_plugin_timer_support_t* extTimerSupport_{nullptr};

    // Active UI Timers
    struct ClapHostTimerEntry {
        clap_id id{0};
        uint32_t periodMs{16};
        std::chrono::steady_clock::time_point nextTick{};
    };
    std::vector<ClapHostTimerEntry> activeTimers_;
    mutable std::mutex timersMutex_;
    clap_id nextTimerId_{1};

    // Real-time zero-allocation processing buffers (supports up to 16 audio ports with up to 8 channels each)
    static constexpr size_t kMaxAudioPorts = 16;
    static constexpr size_t kMaxChannelsPerPort = 8;
    uint32_t inPortCount_{0};
    uint32_t outPortCount_{1};
    std::array<uint32_t, kMaxAudioPorts> inPortChannels_{};
    std::array<uint32_t, kMaxAudioPorts> outPortChannels_{};
    std::array<clap_audio_buffer_t, kMaxAudioPorts> audioInBufs_{};
    std::array<clap_audio_buffer_t, kMaxAudioPorts> audioOutBufs_{};
    std::array<std::array<float*, kMaxChannelsPerPort>, kMaxAudioPorts> inChannelData_{};
    std::array<std::array<float*, kMaxChannelsPerPort>, kMaxAudioPorts> outChannelData_{};
    std::vector<float> silenceBuffer_; // For missing input channels
    std::vector<float> dummyOutBuffer_; // For auxiliary output channels

    clap_input_events_t inEventsList_{};
    clap_output_events_t outEventsList_{};

    // Ring-buffer / pre-allocated event caches for processBlock
    static constexpr size_t kMaxEventsPerBlock = 256;
    std::array<clap_event_note_t, kMaxEventsPerBlock> noteEventsCache_{};
    std::array<clap_event_param_value_t, kMaxEventsPerBlock> paramEventsCache_{};
    std::array<const clap_event_header_t*, kMaxEventsPerBlock> eventHeadersCache_{};
    uint32_t activeEventCount_{0};

    // Thread-safe pending events queue populated by handleEvent
    struct PendingEvent {
        enum class Type { NoteOn, NoteOff, SetParam } type;
        uint8_t note{60};
        float velocity{0.8f};
        int16_t channel{0};
        clap_id paramId{0};
        double paramValue{0.0};
    };
    static constexpr size_t kQueueCapacity = 512;
    std::array<PendingEvent, kQueueCapacity> pendingQueue_{};
    std::atomic<uint32_t> queueWritePos_{0};
    std::atomic<uint32_t> queueReadPos_{0};

    int64_t steadyTime_{0};
    std::atomic<bool> isActivated_{false};
    std::atomic<bool> isProcessing_{false};
    std::atomic<bool> callbackRequested_{false};
    double lastSampleRate_{0.0};
    uint32_t lastBlockSize_{0};

    // GUI state
    bool isGuiOpen_{false};
    void* guiHwnd_{nullptr};
    bool isEmbeddedGuiOpen_{false};
    void* embeddedContainerHwnd_{nullptr};
    int lastEmbeddedX_{-1};
    int lastEmbeddedY_{-1};
    int lastEmbeddedW_{-1};
    int lastEmbeddedH_{-1};
};

} // namespace eatsbits::audio
