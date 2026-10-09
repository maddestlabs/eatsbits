#include "eatsbits/audio/graph/nodes/clap_audio_node.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eatsbits::audio {

// Helpers for CLAP stream I/O for state save/restore
struct MemStreamWriter {
    std::vector<uint8_t> buffer;

    static int64_t CLAP_ABI write(const clap_ostream* stream, const void* data, uint64_t size) {
        auto* self = static_cast<MemStreamWriter*>(stream->ctx);
        const auto* bytes = static_cast<const uint8_t*>(data);
        self->buffer.insert(self->buffer.end(), bytes, bytes + size);
        return static_cast<int64_t>(size);
    }

    clap_ostream makeOStream() {
        return clap_ostream{
            .ctx = this,
            .write = write
        };
    }
};

struct MemStreamReader {
    const uint8_t* data{nullptr};
    size_t size{0};
    size_t readPos{0};

    static int64_t CLAP_ABI read(const clap_istream* stream, void* buffer, uint64_t count) {
        auto* self = static_cast<MemStreamReader*>(stream->ctx);
        if (self->readPos >= self->size) return 0;
        size_t available = self->size - self->readPos;
        size_t toRead = std::min(available, static_cast<size_t>(count));
        std::memcpy(buffer, self->data + self->readPos, toRead);
        self->readPos += toRead;
        return static_cast<int64_t>(toRead);
    }

    clap_istream makeIStream() {
        return clap_istream{
            .ctx = this,
            .read = read
        };
    }
};

#if defined(_WIN32)
static LRESULT CALLBACK ClapHostWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLOSE) {
        auto* node = reinterpret_cast<ClapAudioNode*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if (node) {
            node->closeGuiWindow();
            return 0;
        }
    } else if (msg == WM_ERASEBKGND) {
        return 1; // Prevent GDI background repaint flickering over OpenGL/DirectX
    } else if (msg == WM_SIZE) {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        HWND child = GetWindow(hWnd, GW_CHILD);
        if (child) {
            SetWindowPos(child, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        auto* node = reinterpret_cast<ClapAudioNode*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if (node && node->getClapPlugin() && node->getGuiExtension() && node->getGuiExtension()->set_size) {
            node->getGuiExtension()->set_size(node->getClapPlugin(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        }
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}
#endif // _WIN32

#if defined(_MSC_VER)
static bool SafeClapGuiCreate(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin, const char* api, bool isFloating) {
    __try {
        if (extGui && extGui->create && plugin) {
            return extGui->create(plugin, api, isFloating);
        }
        return false;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(stderr, "[CLAP Host] EXCEPTION CAUGHT: Plugin crashed during gui->create()!\n");
        return false;
    }
}

static bool SafeClapGuiSetParent(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin, const clap_window_t* win) {
    __try {
        if (extGui && extGui->set_parent && plugin && win) {
            return extGui->set_parent(plugin, win);
        }
        return false;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(stderr, "[CLAP Host] EXCEPTION CAUGHT: Plugin crashed during gui->set_parent()!\n");
        return false;
    }
}

static bool SafeClapGuiShow(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin) {
    __try {
        if (extGui && extGui->show && plugin) {
            return extGui->show(plugin);
        }
        return false;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(stderr, "[CLAP Host] EXCEPTION CAUGHT: Plugin crashed during gui->show()!\n");
        return false;
    }
}

static bool SafeClapGuiHide(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin) {
    __try {
        if (extGui && extGui->hide && plugin) {
            return extGui->hide(plugin);
        }
        return false;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(stderr, "[CLAP Host] EXCEPTION CAUGHT: Plugin crashed during gui->hide()!\n");
        return false;
    }
}

static void SafeClapGuiDestroy(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin) {
    __try {
        if (extGui && extGui->destroy && plugin) {
            extGui->destroy(plugin);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(stderr, "[CLAP Host] EXCEPTION CAUGHT: Plugin crashed during gui->destroy()!\n");
    }
}
#else
static bool SafeClapGuiCreate(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin, const char* api, bool isFloating) {
    try {
        return (extGui && extGui->create && plugin) ? extGui->create(plugin, api, isFloating) : false;
    } catch (...) {
        return false;
    }
}

static bool SafeClapGuiSetParent(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin, const clap_window_t* win) {
    try {
        return (extGui && extGui->set_parent && plugin && win) ? extGui->set_parent(plugin, win) : false;
    } catch (...) {
        return false;
    }
}

static bool SafeClapGuiShow(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin) {
    try {
        return (extGui && extGui->show && plugin) ? extGui->show(plugin) : false;
    } catch (...) {
        return false;
    }
}

static bool SafeClapGuiHide(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin) {
    try {
        return (extGui && extGui->hide && plugin) ? extGui->hide(plugin) : false;
    } catch (...) {
        return false;
    }
}

static void SafeClapGuiDestroy(const clap_plugin_gui_t* extGui, const clap_plugin_t* plugin) {
    try {
        if (extGui && extGui->destroy && plugin) {
            extGui->destroy(plugin);
        }
    } catch (...) {}
}
#endif

#if !defined(__EMSCRIPTEN__)
static std::atomic<std::thread::id> sMainThreadId{std::thread::id{}};
#endif
static std::vector<ClapAudioNode*> sActiveNodes;
static std::mutex sNodesMutex;

bool ClapAudioNode::isAnyGuiOpen() {
    std::lock_guard<std::mutex> lk(sNodesMutex);
    for (auto* node : sActiveNodes) {
        if (node && (node->isGuiOpen_ || node->isEmbeddedGuiOpen_)) {
            return true;
        }
    }
    return false;
}

void ClapAudioNode::setMainThreadId(std::thread::id id) noexcept {
#if !defined(__EMSCRIPTEN__)
    sMainThreadId.store(id, std::memory_order_relaxed);
#else
    (void)id;
#endif
}

std::thread::id ClapAudioNode::getMainThreadId() noexcept {
#if !defined(__EMSCRIPTEN__)
    return sMainThreadId.load(std::memory_order_relaxed);
#else
    return {};
#endif
}

void ClapAudioNode::pollAllMainThreadCallbacks() {
    std::vector<ClapAudioNode*> nodesCopy;
    {
        std::lock_guard<std::mutex> lk(sNodesMutex);
        nodesCopy = sActiveNodes;
    }
    for (auto* node : nodesCopy) {
        if (node) {
            node->pollMainThreadCallbacks();
        }
    }
}

void ClapAudioNode::pollMainThreadCallbacks() {
    if (callbackRequested_.exchange(false, std::memory_order_acq_rel)) {
        if (plugin_ && plugin_->on_main_thread) {
            plugin_->on_main_thread(plugin_);
        }
    }

    if (extTimerSupport_ && extTimerSupport_->on_timer && plugin_) {
        std::vector<clap_id> firedTimers;
        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lk(timersMutex_);
            for (auto& t : activeTimers_) {
                if (now >= t.nextTick) {
                    firedTimers.push_back(t.id);
                    t.nextTick = now + std::chrono::milliseconds(t.periodMs);
                }
            }
        }
        for (clap_id tid : firedTimers) {
            extTimerSupport_->on_timer(plugin_, tid);
        }
    }

#if defined(_WIN32)
    if (guiHwnd_) {
        MSG msg;
        while (PeekMessageA(&msg, static_cast<HWND>(guiHwnd_), 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    if (embeddedContainerHwnd_) {
        MSG msg;
        while (PeekMessageA(&msg, static_cast<HWND>(embeddedContainerHwnd_), 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
#endif
}

std::shared_ptr<ClapAudioNode> ClapAudioNode::create(
    const std::string& libraryPath,
    const std::string& pluginId,
    std::string nodeName
) {
    auto node = std::make_shared<ClapAudioNode>(nodeName.empty() ? "ClapPlugin" : nodeName);
    if (!node->initFromLibrary(libraryPath, pluginId)) {
        return nullptr;
    }
    return node;
}

ClapAudioNode::ClapAudioNode(std::string name)
    : GraphNode(std::move(name)) {
#if !defined(__EMSCRIPTEN__)
    if (sMainThreadId.load(std::memory_order_relaxed) == std::thread::id{}) {
        sMainThreadId.store(std::this_thread::get_id(), std::memory_order_relaxed);
    }
    mainThreadId_ = sMainThreadId.load(std::memory_order_relaxed);
#endif
    initHostStructure();

    // Initialize CLAP Event lists
    inEventsList_.ctx = this;
    inEventsList_.size = inEventsSize;
    inEventsList_.get = inEventsGet;

    outEventsList_.ctx = this;
    outEventsList_.try_push = outEventsTryPush;

    {
        std::lock_guard<std::mutex> lk(sNodesMutex);
        sActiveNodes.push_back(this);
    }
}

ClapAudioNode::ClapAudioNode(
    const clap_plugin_t* plugin,
    std::shared_ptr<ClapLibraryHandle> libHandle,
    std::string name
) : ClapAudioNode(std::move(name)) {
    if (plugin) {
        attachPlugin(plugin, std::move(libHandle));
    }
}

bool ClapAudioNode::initFromLibrary(const std::string& libraryPath, const std::string& pluginId) {
    std::shared_ptr<ClapLibraryHandle> handle;
    const clap_plugin_t* plugin = ClapPluginLoader::instance().createPluginInstance(
        libraryPath, pluginId, &host_, handle);

    if (!plugin) {
        return false;
    }

    return attachPlugin(plugin, std::move(handle));
}

bool ClapAudioNode::attachPlugin(const clap_plugin_t* plugin, std::shared_ptr<ClapLibraryHandle> libHandle) {
    if (!plugin) return false;

    plugin_ = plugin;
    libHandle_ = std::move(libHandle);
    pluginId_ = plugin_->desc && plugin_->desc->id ? plugin_->desc->id : "";

    if (getName().empty() || getName() == "ClapPlugin") {
        if (plugin_->desc && plugin_->desc->name) {
            setName(plugin_->desc->name);
        }
    }

    // Initialize plugin with host structure
    if (plugin_->init) {
        if (!plugin_->init(plugin_)) {
            plugin_ = nullptr;
            return false;
        }
    }

    queryPluginConfiguration();
    return true;
}

ClapAudioNode::~ClapAudioNode() {
    destroyEmbeddedGui();
    closeGuiWindow();

    {
        std::lock_guard<std::mutex> lk(sNodesMutex);
        auto it = std::find(sActiveNodes.begin(), sActiveNodes.end(), this);
        if (it != sActiveNodes.end()) {
            sActiveNodes.erase(it);
        }
    }

    if (plugin_) {
        if (isProcessing_.load(std::memory_order_relaxed)) {
            if (plugin_->stop_processing) {
                plugin_->stop_processing(plugin_);
            }
            isProcessing_.store(false, std::memory_order_relaxed);
        }
        if (isActivated_.load(std::memory_order_relaxed)) {
            if (plugin_->deactivate) {
                plugin_->deactivate(plugin_);
            }
            isActivated_.store(false, std::memory_order_relaxed);
        }
        if (plugin_->destroy) {
            plugin_->destroy(plugin_);
        }
        plugin_ = nullptr;
    }
}

void ClapAudioNode::initHostStructure() {
#if !defined(__EMSCRIPTEN__)
    if (sMainThreadId.load(std::memory_order_relaxed) == std::thread::id{}) {
        sMainThreadId.store(std::this_thread::get_id(), std::memory_order_relaxed);
    }
    mainThreadId_ = sMainThreadId.load(std::memory_order_relaxed);
#endif

    host_.clap_version = CLAP_VERSION;
    host_.host_data = this;
    host_.name = "Eatsbits Workstation";
    host_.vendor = "Maddest Labs";
    host_.url = "https://github.com/maddestlabs/eatsbits";
    host_.version = "1.0.0";
    host_.get_extension = hostGetExtension;
    host_.request_restart = hostRequestRestart;
    host_.request_process = hostRequestProcess;
    host_.request_callback = hostRequestCallback;

    // Host GUI Extension
    hostGui_.resize_hints_changed = hostGuiResizeHintsChanged;
    hostGui_.request_resize = hostGuiRequestResize;
    hostGui_.request_show = hostGuiRequestShow;
    hostGui_.request_hide = hostGuiRequestHide;
    hostGui_.closed = hostGuiClosed;

    // Host Thread Check Extension
    hostThreadCheck_.is_main_thread = hostThreadCheckIsMainThread;
    hostThreadCheck_.is_audio_thread = hostThreadCheckIsAudioThread;

    // Host Params Extension
    hostParams_.rescan = hostParamsRescan;
    hostParams_.clear = hostParamsClear;
    hostParams_.request_flush = hostParamsRequestFlush;

    // Host Latency Extension
    hostLatency_.changed = hostLatencyChanged;

    // Host State Extension
    hostState_.mark_dirty = hostStateMarkDirty;

    // Host Log Extension
    hostLog_.log = hostLog;

    // Host Audio Ports Extension
    hostAudioPorts_.is_rescan_flag_supported = hostAudioPortsIsRescanFlagSupported;
    hostAudioPorts_.rescan = hostAudioPortsRescan;

    // Host Note Ports Extension
    hostNotePorts_.supported_dialects = hostNotePortsSupportedDialects;
    hostNotePorts_.rescan = hostNotePortsRescan;

    // Host Tail Extension
    hostTail_.changed = hostTailChanged;

    // Host Timer Support Extension
    hostTimerSupport_.register_timer = hostTimerRegister;
    hostTimerSupport_.unregister_timer = hostTimerUnregister;
}

void ClapAudioNode::queryPluginConfiguration() {
    if (!plugin_ || !plugin_->get_extension) return;

    extParams_ = static_cast<const clap_plugin_params_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_PARAMS));
    extState_ = static_cast<const clap_plugin_state_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_STATE));
    extGui_ = static_cast<const clap_plugin_gui_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_GUI));
    extAudioPorts_ = static_cast<const clap_plugin_audio_ports_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_AUDIO_PORTS));
    extNotePorts_ = static_cast<const clap_plugin_note_ports_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_NOTE_PORTS));
    extTimerSupport_ = static_cast<const clap_plugin_timer_support_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_TIMER_SUPPORT));

    // Determine Audio Ports
    inPortCount_ = 0;
    outPortCount_ = 1; // Default stereo out
    if (extAudioPorts_) {
        inPortCount_ = std::min<uint32_t>(static_cast<uint32_t>(kMaxAudioPorts), extAudioPorts_->count(plugin_, true));
        outPortCount_ = std::min<uint32_t>(static_cast<uint32_t>(kMaxAudioPorts), std::max(1u, extAudioPorts_->count(plugin_, false)));
        for (uint32_t i = 0; i < inPortCount_; ++i) {
            clap_audio_port_info_t info{};
            if (extAudioPorts_->get(plugin_, i, true, &info)) {
                inPortChannels_[i] = std::min<uint32_t>(static_cast<uint32_t>(kMaxChannelsPerPort), std::max(1u, info.channel_count));
            } else {
                inPortChannels_[i] = 2;
            }
        }
        for (uint32_t i = 0; i < outPortCount_; ++i) {
            clap_audio_port_info_t info{};
            if (extAudioPorts_->get(plugin_, i, false, &info)) {
                outPortChannels_[i] = std::min<uint32_t>(static_cast<uint32_t>(kMaxChannelsPerPort), std::max(1u, info.channel_count));
            } else {
                outPortChannels_[i] = 2;
            }
        }
    } else {
        inPortChannels_[0] = 2;
        outPortChannels_[0] = 2;
    }

    if (inPortCount_ > 0) {
        addInputPort(2);
        isAudioEffect_ = true;
    } else {
        isAudioEffect_ = false;
    }

    for (uint32_t i = 0; i < outPortCount_; ++i) {
        addOutputPort(2);
    }

    if (plugin_->desc && plugin_->desc->features) {
        for (const char* const* f = plugin_->desc->features; *f; ++f) {
            std::string feat = *f;
            for (char& c : feat) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (feat == "instrument" || feat == "synthesizer" || feat == "sampler") {
                isInstrument_ = true;
                break;
            }
        }
    }
    if (!isAudioEffect_) {
        isInstrument_ = true;
    }
}

void ClapAudioNode::prepare(double sampleRate, uint32_t maxBlockSize) {
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    silenceBuffer_.assign(maxBlockSize, 0.0f);
    dummyOutBuffer_.assign(maxBlockSize, 0.0f);

    if (!plugin_) return;

    if (isActivated_.load(std::memory_order_relaxed)) {
        if (lastSampleRate_ == sampleRate && lastBlockSize_ == maxBlockSize) {
            return; // Idempotent: already activated with identical configuration
        }
        if (plugin_->deactivate) {
            plugin_->deactivate(plugin_);
        }
        isActivated_.store(false, std::memory_order_relaxed);
    }

    if (!isActivated_.load(std::memory_order_relaxed) && plugin_->activate) {
        if (plugin_->activate(plugin_, sampleRate, 32, maxBlockSize)) {
            isActivated_.store(true, std::memory_order_relaxed);
            lastSampleRate_ = sampleRate;
            lastBlockSize_ = maxBlockSize;
        }
    }
}

void ClapAudioNode::reset() noexcept {
    if (plugin_ && isProcessing_.load(std::memory_order_relaxed)) {
        if (plugin_->reset) {
            plugin_->reset(plugin_);
        }
    }
    queueReadPos_.store(queueWritePos_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    activeEventCount_ = 0;
}

void ClapAudioNode::processBlock(uint32_t numFrames) noexcept {
#if !defined(__EMSCRIPTEN__)
    audioThreadId_.store(std::this_thread::get_id(), std::memory_order_relaxed);
#endif

    float* outL = getOutputBuffer(0, 0);
    float* outR = getOutputBuffer(0, 1);

    if (!outL || !outR) return;

    if (!enabled_ || !plugin_ || !isActivated_.load(std::memory_order_relaxed)) {
        std::fill_n(outL, numFrames, 0.0f);
        std::fill_n(outR, numFrames, 0.0f);
        return;
    }

    // Call start_processing on audio thread if not already running
    if (!isProcessing_.load(std::memory_order_relaxed)) {
        if (plugin_->start_processing) {
            plugin_->start_processing(plugin_);
        }
        isProcessing_.store(true, std::memory_order_relaxed);
    }

    // 1. Drain pending events into zero-allocation pre-allocated event caches
    activeEventCount_ = 0;
    uint32_t rPos = queueReadPos_.load(std::memory_order_relaxed);
    const uint32_t wPos = queueWritePos_.load(std::memory_order_acquire);

    while (rPos != wPos && activeEventCount_ < kMaxEventsPerBlock) {
        const auto& pending = pendingQueue_[rPos % kQueueCapacity];
        rPos++;

        if (pending.type == PendingEvent::Type::NoteOn || pending.type == PendingEvent::Type::NoteOff) {
            auto& ne = noteEventsCache_[activeEventCount_];
            ne.header.size = sizeof(clap_event_note_t);
            ne.header.time = 0;
            ne.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ne.header.type = (pending.type == PendingEvent::Type::NoteOn) ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF;
            ne.header.flags = 0;
            ne.note_id = -1;
            ne.port_index = 0;
            ne.channel = pending.channel;
            ne.key = static_cast<int16_t>(pending.note);
            ne.velocity = static_cast<double>(pending.velocity);

            eventHeadersCache_[activeEventCount_] = &ne.header;
            activeEventCount_++;
        } else if (pending.type == PendingEvent::Type::SetParam) {
            auto& pe = paramEventsCache_[activeEventCount_];
            pe.header.size = sizeof(clap_event_param_value_t);
            pe.header.time = 0;
            pe.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            pe.header.type = CLAP_EVENT_PARAM_VALUE;
            pe.header.flags = 0;
            pe.param_id = pending.paramId;
            pe.cookie = nullptr;
            pe.note_id = -1;
            pe.port_index = -1;
            pe.channel = -1;
            pe.key = -1;
            pe.value = pending.paramValue;

            eventHeadersCache_[activeEventCount_] = &pe.header;
            activeEventCount_++;
        }
    }
    queueReadPos_.store(rPos, std::memory_order_release);

    // 2. Map planar audio buffers across all ports
    const float* inL = (numInputPorts() > 0) ? getInputBuffer(0, 0) : nullptr;
    const float* inR = (numInputPorts() > 0) ? getInputBuffer(0, 1) : nullptr;

    for (uint32_t p = 0; p < inPortCount_; ++p) {
        uint32_t chs = inPortChannels_[p];
        for (uint32_t c = 0; c < chs; ++c) {
            if (p == 0) {
                if (c == 0) inChannelData_[0][0] = inL ? const_cast<float*>(inL) : silenceBuffer_.data();
                else if (c == 1) inChannelData_[0][1] = inR ? const_cast<float*>(inR) : (inL ? const_cast<float*>(inL) : silenceBuffer_.data());
                else inChannelData_[0][c] = silenceBuffer_.data();
            } else {
                inChannelData_[p][c] = silenceBuffer_.data();
            }
        }
        audioInBufs_[p].data32 = inChannelData_[p].data();
        audioInBufs_[p].data64 = nullptr;
        audioInBufs_[p].channel_count = chs;
        audioInBufs_[p].latency = 0;
        audioInBufs_[p].constant_mask = 0;
    }

    for (uint32_t p = 0; p < outPortCount_; ++p) {
        uint32_t chs = outPortChannels_[p];
        for (uint32_t c = 0; c < chs; ++c) {
            if (p == 0) {
                if (c == 0) outChannelData_[0][0] = outL;
                else if (c == 1) outChannelData_[0][1] = outR;
                else outChannelData_[0][c] = dummyOutBuffer_.data();
            } else {
                outChannelData_[p][c] = dummyOutBuffer_.data();
            }
        }
        audioOutBufs_[p].data32 = outChannelData_[p].data();
        audioOutBufs_[p].data64 = nullptr;
        audioOutBufs_[p].channel_count = chs;
        audioOutBufs_[p].latency = 0;
        audioOutBufs_[p].constant_mask = 0;
    }

    // 3. Populate Transport context for tempo-synced DSP
    transport_.header.size = sizeof(clap_event_transport_t);
    transport_.header.type = CLAP_EVENT_TRANSPORT;
    transport_.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    transport_.header.time = 0;
    transport_.header.flags = 0;
    transport_.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE |
                       CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
    if (isPlaying_) {
        transport_.flags |= CLAP_TRANSPORT_IS_PLAYING;
    }
    transport_.tempo = bpm_;
    transport_.tempo_inc = 0.0;
    transport_.song_pos_seconds = static_cast<double>(steadyTime_) / (sampleRate_ > 0.0 ? sampleRate_ : 44100.0);
    transport_.song_pos_beats = static_cast<clap_beattime>((transport_.song_pos_seconds * (bpm_ / 60.0)) * CLAP_BEATTIME_FACTOR);
    transport_.tsig_num = 4;
    transport_.tsig_denom = 4;

    // 4. Dispatch CLAP process block
    clap_process_t process{};
    process.steady_time = steadyTime_;
    process.frames_count = numFrames;
    process.transport = &transport_;
    process.audio_inputs = (inPortCount_ > 0) ? audioInBufs_.data() : nullptr;
    process.audio_inputs_count = inPortCount_;
    process.audio_outputs = audioOutBufs_.data();
    process.audio_outputs_count = outPortCount_;
    process.in_events = &inEventsList_;
    process.out_events = &outEventsList_;

    if (plugin_->process) {
        plugin_->process(plugin_, &process);
    }

    steadyTime_ += numFrames;
}

void ClapAudioNode::handleEvent(const AudioEvent& event) noexcept {
    switch (event.type) {
        case AudioEventType::NoteOn:
            noteOn(event.note, event.velocity, event.channel);
            break;
        case AudioEventType::NoteOff:
            noteOff(event.note, event.channel);
            break;
        case AudioEventType::AllNotesOff:
            allNotesOff();
            break;
        case AudioEventType::SetParameter:
            setParameter(event.paramId, event.paramValue);
            break;
        default:
            break;
    }
}

void ClapAudioNode::noteOn(uint8_t note, float velocity, int16_t channel) noexcept {
    uint32_t wPos = queueWritePos_.load(std::memory_order_relaxed);
    auto& ev = pendingQueue_[wPos % kQueueCapacity];
    ev.type = PendingEvent::Type::NoteOn;
    ev.note = note;
    ev.velocity = velocity;
    ev.channel = channel;
    queueWritePos_.store(wPos + 1, std::memory_order_release);
}

void ClapAudioNode::noteOff(uint8_t note, int16_t channel) noexcept {
    uint32_t wPos = queueWritePos_.load(std::memory_order_relaxed);
    auto& ev = pendingQueue_[wPos % kQueueCapacity];
    ev.type = PendingEvent::Type::NoteOff;
    ev.note = note;
    ev.velocity = 0.0f;
    ev.channel = channel;
    queueWritePos_.store(wPos + 1, std::memory_order_release);
}

void ClapAudioNode::allNotesOff() noexcept {
    for (uint8_t n = 0; n < 128; ++n) {
        noteOff(n, 0);
    }
}

void ClapAudioNode::setParameter(uint32_t paramId, float value) noexcept {
    setParamValue(static_cast<clap_id>(paramId), static_cast<double>(value));
}

void ClapAudioNode::setParamValue(clap_id paramId, double value) noexcept {
    uint32_t wPos = queueWritePos_.load(std::memory_order_relaxed);
    auto& ev = pendingQueue_[wPos % kQueueCapacity];
    ev.type = PendingEvent::Type::SetParam;
    ev.paramId = paramId;
    ev.paramValue = value;
    queueWritePos_.store(wPos + 1, std::memory_order_release);

    if (extParams_ && extParams_->flush) {
        clap_event_param_value_t pe{};
        pe.header.size = sizeof(clap_event_param_value_t);
        pe.header.time = 0;
        pe.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        pe.header.type = CLAP_EVENT_PARAM_VALUE;
        pe.header.flags = 0;
        pe.param_id = paramId;
        pe.cookie = nullptr;
        pe.note_id = -1;
        pe.port_index = -1;
        pe.channel = -1;
        pe.key = -1;
        pe.value = value;

        const clap_event_header_t* hdr = &pe.header;
        clap_input_events_t flushIn{
            .ctx = const_cast<void*>(static_cast<const void*>(&hdr)),
            .size = [](const clap_input_events_t*) -> uint32_t { return 1; },
            .get = [](const clap_input_events_t* l, uint32_t) -> const clap_event_header_t* {
                return *static_cast<const clap_event_header_t**>(l->ctx);
            }
        };
        clap_output_events_t flushOut{
            .ctx = nullptr,
            .try_push = [](const clap_output_events_t*, const clap_event_header_t*) -> bool { return true; }
        };
        extParams_->flush(plugin_, &flushIn, &flushOut);
    }
}

double ClapAudioNode::getParamValue(clap_id paramId) const noexcept {
    if (!plugin_ || !extParams_ || !extParams_->get_value) return 0.0;
    double val = 0.0;
    if (extParams_->get_value(plugin_, paramId, &val)) {
        return val;
    }
    return 0.0;
}

uint32_t ClapAudioNode::getParamCount() const noexcept {
    if (!plugin_ || !extParams_ || !extParams_->count) return 0;
    return extParams_->count(plugin_);
}

std::vector<ClapParamDef> ClapAudioNode::getParameterList() const {
    std::vector<ClapParamDef> params;
    if (!plugin_ || !extParams_ || !extParams_->count || !extParams_->get_info) {
        return params;
    }

    uint32_t count = extParams_->count(plugin_);
    params.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        clap_param_info_t info{};
        if (extParams_->get_info(plugin_, i, &info)) {
            ClapParamDef def{};
            def.id = info.id;
            def.name = info.name ? info.name : "";
            def.module = info.module ? info.module : "";
            def.minValue = info.min_value;
            def.maxValue = info.max_value;
            def.defaultValue = info.default_value;
            def.flags = info.flags;
            def.currentValue = getParamValue(info.id);
            params.push_back(std::move(def));
        }
    }

    return params;
}

void ClapAudioNode::reflectParameters() {
    (void)getParameterList();
}

std::vector<uint8_t> ClapAudioNode::saveState() const {
    if (!plugin_ || !extState_ || !extState_->save) {
        return {};
    }
    MemStreamWriter writer;
    clap_ostream stream = writer.makeOStream();
    if (extState_->save(plugin_, &stream)) {
        return writer.buffer;
    }
    return {};
}

bool ClapAudioNode::loadState(const std::vector<uint8_t>& stateData) {
    if (!plugin_ || !extState_ || !extState_->load || stateData.empty()) {
        return false;
    }
    MemStreamReader reader;
    reader.data = stateData.data();
    reader.size = stateData.size();
    clap_istream stream = reader.makeIStream();
    return extState_->load(plugin_, &stream);
}

bool ClapAudioNode::hasGui() const noexcept {
    return extGui_ != nullptr;
}

bool ClapAudioNode::openGuiWindow() {
    if (!plugin_ || !extGui_) {
        return false;
    }
#if !defined(__EMSCRIPTEN__)
    if (sMainThreadId.load(std::memory_order_relaxed) != std::thread::id{}) {
        if (std::this_thread::get_id() != sMainThreadId.load(std::memory_order_relaxed)) {
            std::fprintf(stderr, "[CLAP Host] Warning: openGuiWindow called outside main thread!\n");
            return false;
        }
    }
#endif
    if (isGuiOpen_) {
#if defined(_WIN32)
        if (guiHwnd_) {
            ShowWindow(static_cast<HWND>(guiHwnd_), SW_RESTORE);
            SetForegroundWindow(static_cast<HWND>(guiHwnd_));
        }
#endif
        return true;
    }

#if defined(_WIN32)
    const char* api = CLAP_WINDOW_API_WIN32;
    bool supEmbedded = extGui_->is_api_supported ? extGui_->is_api_supported(plugin_, api, false) : false;
    bool supFloating = extGui_->is_api_supported ? extGui_->is_api_supported(plugin_, api, true) : false;

    if (!supEmbedded && !supFloating) {
        return false;
    }

    bool isFloating = false;
    if (extGui_->get_preferred_api) {
        const char* preferredApi = nullptr;
        bool preferredFloating = false;
        if (extGui_->get_preferred_api(plugin_, &preferredApi, &preferredFloating)) {
            if (preferredApi && std::strcmp(preferredApi, CLAP_WINDOW_API_WIN32) == 0) {
                if (preferredFloating && supFloating) isFloating = true;
                else if (!preferredFloating && supEmbedded) isFloating = false;
            }
        }
    }
    if (!supEmbedded && supFloating) {
        isFloating = true;
    } else if (supEmbedded && !supFloating) {
        isFloating = false;
    }

    std::string title = getName() + " (CLAP)";

    if (isFloating) {
        if (!SafeClapGuiCreate(extGui_, plugin_, api, true)) {
            return false;
        }
        if (extGui_->suggest_title) {
            extGui_->suggest_title(plugin_, title.c_str());
        }
        if (!SafeClapGuiShow(extGui_, plugin_)) {
            SafeClapGuiDestroy(extGui_, plugin_);
            return false;
        }
        isGuiOpen_ = true;
        return true;
    }

    // Embedded mode inside host top-level floating window
    static bool sClassRegistered = false;
    static const char* kClassName = "EatsbitsClapHostWindow";
    if (!sClassRegistered) {
        WNDCLASSA wc{};
        wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ClapHostWindowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        RegisterClassA(&wc);
        sClassRegistered = true;
    }

    if (!SafeClapGuiCreate(extGui_, plugin_, api, false)) {
        return false;
    }

    if (extGui_->set_scale) {
        extGui_->set_scale(plugin_, 1.0);
    }

    uint32_t width = 800;
    uint32_t height = 500;
    if (extGui_->get_size) {
        extGui_->get_size(plugin_, &width, &height);
    }

    RECT rc = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_APPWINDOW);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    HWND hWnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        kClassName,
        title.c_str(),
        style | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    if (!hWnd) {
        SafeClapGuiDestroy(extGui_, plugin_);
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    HDC hdc = GetDC(hWnd);
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf > 0) {
        SetPixelFormat(hdc, pf, &pfd);
    }
    ReleaseDC(hWnd, hdc);

    SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    guiHwnd_ = hWnd;

    clap_window_t win{};
    win.api = CLAP_WINDOW_API_WIN32;
    win.win32 = static_cast<clap_hwnd>(hWnd);

    if (!SafeClapGuiSetParent(extGui_, plugin_, &win)) {
        SafeClapGuiDestroy(extGui_, plugin_);
        DestroyWindow(hWnd);
        guiHwnd_ = nullptr;
        return false;
    }

    if (extGui_->set_size) {
        extGui_->set_size(plugin_, width, height);
    }

    if (!SafeClapGuiShow(extGui_, plugin_)) {
        SafeClapGuiDestroy(extGui_, plugin_);
        DestroyWindow(hWnd);
        guiHwnd_ = nullptr;
        return false;
    }

    isGuiOpen_ = true;
    return true;
#else
    return false;
#endif
}

void ClapAudioNode::closeGuiWindow() {
    if (!isGuiOpen_) return;
#if !defined(__EMSCRIPTEN__)
    if (sMainThreadId.load(std::memory_order_relaxed) != std::thread::id{}) {
        if (std::this_thread::get_id() != sMainThreadId.load(std::memory_order_relaxed)) {
            std::fprintf(stderr, "[CLAP Host] Warning: closeGuiWindow called outside main thread!\n");
        }
    }
#endif

    if (plugin_ && extGui_) {
        SafeClapGuiHide(extGui_, plugin_);
        SafeClapGuiDestroy(extGui_, plugin_);
    }

#if defined(_WIN32)
    if (guiHwnd_) {
        HWND hWnd = static_cast<HWND>(guiHwnd_);
        SetWindowLongPtrA(hWnd, GWLP_USERDATA, 0);
        DestroyWindow(hWnd);
        guiHwnd_ = nullptr;
    }
#endif

    isGuiOpen_ = false;
}

bool ClapAudioNode::openEmbeddedGui(void* parentHwnd, int x, int y, int w, int h) {
#if defined(_WIN32)
    if (!plugin_ || !extGui_) return false;
    if (sMainThreadId.load(std::memory_order_relaxed) != std::thread::id{}) {
        if (std::this_thread::get_id() != sMainThreadId.load(std::memory_order_relaxed)) {
            std::fprintf(stderr, "[CLAP Host] Warning: openEmbeddedGui called outside main thread!\n");
            return false;
        }
    }

    HWND parent = static_cast<HWND>(parentHwnd);
    if (!parent) return false;

    // If a floating window is open, close it cleanly first!
    if (guiHwnd_) {
        closeGuiWindow();
    }

    // If the embedded container was already created, reuse it!
    if (embeddedContainerHwnd_) {
        setEmbeddedGuiBounds(x, y, w, h);
        showEmbeddedGui();
        return true;
    }

    const char* api = CLAP_WINDOW_API_WIN32;
    if (!extGui_->is_api_supported || !extGui_->is_api_supported(plugin_, api, false)) {
        return openGuiWindow();
    }

    LONG parentStyle = GetWindowLongA(parent, GWL_STYLE);
    if (!(parentStyle & WS_CLIPCHILDREN)) {
        SetWindowLongA(parent, GWL_STYLE, parentStyle | WS_CLIPCHILDREN);
    }

    static bool sContainerClassRegistered = false;
    static const char* kContainerClassName = "EatsbitsClapContainer";
    if (!sContainerClassRegistered) {
        WNDCLASSA wc{};
        wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ClapHostWindowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kContainerClassName;
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        RegisterClassA(&wc);
        sContainerClassRegistered = true;
    }

    HWND container = CreateWindowExA(
        0,
        kContainerClassName,
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, w, h,
        parent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );

    if (!container) return false;

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    HDC hdc = GetDC(container);
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf > 0) {
        SetPixelFormat(hdc, pf, &pfd);
    }
    ReleaseDC(container, hdc);

    SetWindowLongPtrA(container, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    embeddedContainerHwnd_ = container;

    if (!SafeClapGuiCreate(extGui_, plugin_, api, false)) {
        DestroyWindow(container);
        embeddedContainerHwnd_ = nullptr;
        return false;
    }

    if (extGui_->set_scale) {
        extGui_->set_scale(plugin_, 1.0);
    }

    uint32_t targetW = static_cast<uint32_t>(w);
    uint32_t targetH = static_cast<uint32_t>(h);
    if (extGui_->get_size) {
        uint32_t pw = 0, ph = 0;
        if (extGui_->get_size(plugin_, &pw, &ph) && pw > 0 && ph > 0) {
            targetW = pw;
            targetH = ph;
        }
    }

    if (extGui_->can_resize && extGui_->can_resize(plugin_)) {
        targetW = static_cast<uint32_t>(w);
        targetH = static_cast<uint32_t>(h);
    }

    clap_window_t win{};
    win.api = CLAP_WINDOW_API_WIN32;
    win.win32 = static_cast<clap_hwnd>(container);

    if (!SafeClapGuiSetParent(extGui_, plugin_, &win)) {
        SafeClapGuiDestroy(extGui_, plugin_);
        DestroyWindow(container);
        embeddedContainerHwnd_ = nullptr;
        return false;
    }

    if (extGui_->set_size) {
        extGui_->set_size(plugin_, targetW, targetH);
    }

    if (!SafeClapGuiShow(extGui_, plugin_)) {
        SafeClapGuiDestroy(extGui_, plugin_);
        DestroyWindow(container);
        embeddedContainerHwnd_ = nullptr;
        return false;
    }

    HWND child = GetWindow(container, GW_CHILD);
    if (child) {
        SetWindowPos(child, HWND_TOP, 0, 0, w, h, SWP_SHOWWINDOW);
    }
    SetWindowPos(container, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);

    lastEmbeddedX_ = x;
    lastEmbeddedY_ = y;
    lastEmbeddedW_ = w;
    lastEmbeddedH_ = h;

    isEmbeddedGuiOpen_ = true;
    isGuiOpen_ = true;
    return true;
#else
    (void)parentHwnd; (void)x; (void)y; (void)w; (void)h;
    return false;
#endif
}

void ClapAudioNode::setEmbeddedGuiBounds(int x, int y, int w, int h) {
#if defined(_WIN32)
    if (!embeddedContainerHwnd_) return;
    if (x == lastEmbeddedX_ && y == lastEmbeddedY_ && w == lastEmbeddedW_ && h == lastEmbeddedH_) {
        return;
    }
    lastEmbeddedX_ = x;
    lastEmbeddedY_ = y;
    lastEmbeddedW_ = w;
    lastEmbeddedH_ = h;

    HWND container = static_cast<HWND>(embeddedContainerHwnd_);
    SetWindowPos(container, HWND_TOP, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    HWND child = GetWindow(container, GW_CHILD);
    if (child) {
        SetWindowPos(child, HWND_TOP, 0, 0, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (plugin_ && extGui_ && extGui_->set_size) {
        extGui_->set_size(plugin_, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }
#else
    (void)x; (void)y; (void)w; (void)h;
#endif
}

void ClapAudioNode::showEmbeddedGui() {
#if defined(_WIN32)
    if (embeddedContainerHwnd_) {
        HWND container = static_cast<HWND>(embeddedContainerHwnd_);
        ShowWindow(container, SW_SHOW);
        HWND child = GetWindow(container, GW_CHILD);
        if (child) {
            ShowWindow(child, SW_SHOW);
        }
    }
    if (plugin_ && extGui_) {
        SafeClapGuiShow(extGui_, plugin_);
    }
    isEmbeddedGuiOpen_ = true;
    isGuiOpen_ = true;
#endif
}

void ClapAudioNode::hideEmbeddedGui() {
#if defined(_WIN32)
    if (embeddedContainerHwnd_) {
        HWND container = static_cast<HWND>(embeddedContainerHwnd_);
        ShowWindow(container, SW_HIDE);
        HWND child = GetWindow(container, GW_CHILD);
        if (child) {
            ShowWindow(child, SW_HIDE);
        }
    }
    if (plugin_ && extGui_) {
        SafeClapGuiHide(extGui_, plugin_);
    }
    isEmbeddedGuiOpen_ = false;
#endif
}

void ClapAudioNode::destroyEmbeddedGui() {
#if defined(_WIN32)
    if (!embeddedContainerHwnd_) return;
    if (sMainThreadId.load(std::memory_order_relaxed) != std::thread::id{}) {
        if (std::this_thread::get_id() != sMainThreadId.load(std::memory_order_relaxed)) {
            std::fprintf(stderr, "[CLAP Host] Warning: destroyEmbeddedGui called outside main thread!\n");
        }
    }
    hideEmbeddedGui();
    if (plugin_ && extGui_) {
        SafeClapGuiDestroy(extGui_, plugin_);
    }
    HWND container = static_cast<HWND>(embeddedContainerHwnd_);
    SetWindowLongPtrA(container, GWLP_USERDATA, 0);
    DestroyWindow(container);
    embeddedContainerHwnd_ = nullptr;
    isEmbeddedGuiOpen_ = false;
    lastEmbeddedX_ = -1;
    lastEmbeddedY_ = -1;
    lastEmbeddedW_ = -1;
    lastEmbeddedH_ = -1;
#endif
}

// ============================================================================
// CLAP Host Callbacks
// ============================================================================

const void* CLAP_ABI ClapAudioNode::hostGetExtension(const clap_host_t* host, const char* extension_id) {
    if (!host || !host->host_data || !extension_id) return nullptr;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);

    // std::printf("[CLAP Host] hostGetExtension: %s\n", extension_id);

    if (std::strcmp(extension_id, CLAP_EXT_GUI) == 0) return &self->hostGui_;
    if (std::strcmp(extension_id, CLAP_EXT_THREAD_CHECK) == 0) return &self->hostThreadCheck_;
    if (std::strcmp(extension_id, CLAP_EXT_PARAMS) == 0) return &self->hostParams_;
    if (std::strcmp(extension_id, CLAP_EXT_LATENCY) == 0) return &self->hostLatency_;
    if (std::strcmp(extension_id, CLAP_EXT_STATE) == 0) return &self->hostState_;
    if (std::strcmp(extension_id, CLAP_EXT_LOG) == 0) return &self->hostLog_;
    if (std::strcmp(extension_id, CLAP_EXT_AUDIO_PORTS) == 0) return &self->hostAudioPorts_;
    if (std::strcmp(extension_id, CLAP_EXT_NOTE_PORTS) == 0) return &self->hostNotePorts_;
    if (std::strcmp(extension_id, CLAP_EXT_TAIL) == 0) return &self->hostTail_;
    if (std::strcmp(extension_id, CLAP_EXT_TIMER_SUPPORT) == 0) return &self->hostTimerSupport_;

    return nullptr;
}

bool CLAP_ABI ClapAudioNode::hostTimerRegister(const clap_host_t* host, uint32_t period_ms, clap_id* timer_id) {
    if (!host || !host->host_data || !timer_id) return false;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    std::lock_guard<std::mutex> lk(self->timersMutex_);
    clap_id id = self->nextTimerId_++;
    *timer_id = id;
    uint32_t period = std::max<uint32_t>(10, period_ms);
    self->activeTimers_.push_back(ClapHostTimerEntry{
        .id = id,
        .periodMs = period,
        .nextTick = std::chrono::steady_clock::now() + std::chrono::milliseconds(period)
    });
    return true;
}

bool CLAP_ABI ClapAudioNode::hostTimerUnregister(const clap_host_t* host, clap_id timer_id) {
    if (!host || !host->host_data) return false;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    std::lock_guard<std::mutex> lk(self->timersMutex_);
    auto it = std::remove_if(self->activeTimers_.begin(), self->activeTimers_.end(),
                             [timer_id](const ClapHostTimerEntry& e) { return e.id == timer_id; });
    self->activeTimers_.erase(it, self->activeTimers_.end());
    return true;
}

bool CLAP_ABI ClapAudioNode::hostThreadCheckIsMainThread(const clap_host_t* host) {
#if !defined(__EMSCRIPTEN__)
    const auto currentId = std::this_thread::get_id();
    const auto mainId = sMainThreadId.load(std::memory_order_relaxed);
    if (mainId != std::thread::id{}) {
        return currentId == mainId;
    }
    if (!host || !host->host_data) return true;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    return currentId == self->mainThreadId_;
#else
    (void)host;
    return true;
#endif
}

bool CLAP_ABI ClapAudioNode::hostThreadCheckIsAudioThread(const clap_host_t* host) {
#if !defined(__EMSCRIPTEN__)
    const auto currentId = std::this_thread::get_id();
    const auto mainId = sMainThreadId.load(std::memory_order_relaxed);
    if (mainId != std::thread::id{} && currentId == mainId) {
        return false;
    }
    if (!host || !host->host_data) {
        return (mainId != std::thread::id{}) ? (currentId != mainId) : false;
    }
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    const auto audioId = self->audioThreadId_.load(std::memory_order_relaxed);
    if (audioId != std::thread::id{}) {
        return currentId == audioId;
    }
    return (mainId != std::thread::id{}) ? (currentId != mainId) : false;
#else
    (void)host;
    return false;
#endif
}

void CLAP_ABI ClapAudioNode::hostParamsRescan(const clap_host_t* host, clap_param_rescan_flags /*flags*/) {
    if (!host || !host->host_data) return;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    self->reflectParameters();
}

void CLAP_ABI ClapAudioNode::hostParamsClear(const clap_host_t* /*host*/, clap_id /*param_id*/, clap_param_clear_flags /*flags*/) {}

void CLAP_ABI ClapAudioNode::hostParamsRequestFlush(const clap_host_t* /*host*/) {}

void CLAP_ABI ClapAudioNode::hostLatencyChanged(const clap_host_t* host) {
    if (!host || !host->host_data) return;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    if (self->plugin_ && self->plugin_->get_extension) {
        auto* lat = static_cast<const clap_plugin_latency_t*>(self->plugin_->get_extension(self->plugin_, CLAP_EXT_LATENCY));
        if (lat && lat->get) {
            uint32_t latencySamples = lat->get(self->plugin_);
            (void)latencySamples;
        }
    }
}

void CLAP_ABI ClapAudioNode::hostStateMarkDirty(const clap_host_t* /*host*/) {}

void CLAP_ABI ClapAudioNode::hostLog(const clap_host_t* /*host*/, clap_log_severity severity, const char* msg) {
    if (msg) {
        std::printf("[CLAP Plugin Log %d] %s\n", severity, msg);
    }
}

bool CLAP_ABI ClapAudioNode::hostAudioPortsIsRescanFlagSupported(const clap_host_t* /*host*/, uint32_t /*flag*/) {
    return false;
}

void CLAP_ABI ClapAudioNode::hostAudioPortsRescan(const clap_host_t* /*host*/, uint32_t /*flags*/) {}

uint32_t CLAP_ABI ClapAudioNode::hostNotePortsSupportedDialects(const clap_host_t* /*host*/) {
    return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
}

void CLAP_ABI ClapAudioNode::hostNotePortsRescan(const clap_host_t* /*host*/, uint32_t /*flags*/) {}

void CLAP_ABI ClapAudioNode::hostTailChanged(const clap_host_t* /*host*/) {}

void CLAP_ABI ClapAudioNode::hostRequestRestart(const clap_host_t* host) {
    if (!host || !host->host_data) return;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    if (self->plugin_ && self->plugin_->deactivate && self->plugin_->activate) {
        self->plugin_->deactivate(self->plugin_);
        self->plugin_->activate(self->plugin_, self->sampleRate_, 32, self->maxBlockSize_);
    }
}

void CLAP_ABI ClapAudioNode::hostRequestProcess(const clap_host_t* /*host*/) {}

void CLAP_ABI ClapAudioNode::hostRequestCallback(const clap_host_t* host) {
    if (!host || !host->host_data) return;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
#if !defined(__EMSCRIPTEN__)
    const auto currentId = std::this_thread::get_id();
    const auto mainId = sMainThreadId.load(std::memory_order_relaxed);

    if (mainId != std::thread::id{} && currentId == mainId) {
        if (self->plugin_ && self->plugin_->on_main_thread) {
            self->plugin_->on_main_thread(self->plugin_);
        }
    } else {
        self->callbackRequested_.store(true, std::memory_order_release);
    }
#else
    if (self->plugin_ && self->plugin_->on_main_thread) {
        self->plugin_->on_main_thread(self->plugin_);
    }
#endif
}

void CLAP_ABI ClapAudioNode::hostGuiResizeHintsChanged(const clap_host_t* host) {
    if (!host || !host->host_data) return;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    if (self->plugin_ && self->extGui_ && self->extGui_->get_resize_hints) {
        clap_gui_resize_hints_t hints{};
        self->extGui_->get_resize_hints(self->plugin_, &hints);
    }
}

bool CLAP_ABI ClapAudioNode::hostGuiRequestResize(const clap_host_t* host, uint32_t width, uint32_t height) {
    if (!host || !host->host_data) return false;
#if defined(_WIN32)
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    if (self->guiHwnd_) {
        SetWindowPos(static_cast<HWND>(self->guiHwnd_), nullptr, 0, 0,
                     static_cast<int>(width), static_cast<int>(height),
                     SWP_NOMOVE | SWP_NOZORDER);
        return true;
    }
#endif
    return false;
}

bool CLAP_ABI ClapAudioNode::hostGuiRequestShow(const clap_host_t* host) {
    if (!host || !host->host_data) return false;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    return self->openGuiWindow();
}

bool CLAP_ABI ClapAudioNode::hostGuiRequestHide(const clap_host_t* host) {
    if (!host || !host->host_data) return false;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    self->closeGuiWindow();
    return true;
}

void CLAP_ABI ClapAudioNode::hostGuiClosed(const clap_host_t* host, bool /*was_destroyed*/) {
    if (!host || !host->host_data) return;
    auto* self = static_cast<ClapAudioNode*>(host->host_data);
    self->isGuiOpen_ = false;
    self->guiHwnd_ = nullptr;
}

// ============================================================================
// Real-Time Event Callbacks
// ============================================================================

uint32_t CLAP_ABI ClapAudioNode::inEventsSize(const clap_input_events_t* list) {
    auto* self = static_cast<const ClapAudioNode*>(list->ctx);
    return self->activeEventCount_;
}

const clap_event_header_t* CLAP_ABI ClapAudioNode::inEventsGet(const clap_input_events_t* list, uint32_t index) {
    auto* self = static_cast<const ClapAudioNode*>(list->ctx);
    if (index >= self->activeEventCount_) return nullptr;
    return self->eventHeadersCache_[index];
}

bool CLAP_ABI ClapAudioNode::outEventsTryPush(const clap_output_events_t* /*list*/, const clap_event_header_t* /*event*/) {
    return true;
}

} // namespace eatsbits::audio
