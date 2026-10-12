#ifndef EATS_CLAP_PLUGIN_SHELL_HPP
#define EATS_CLAP_PLUGIN_SHELL_HPP

#include <clap/clap.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

namespace eatsbits::export_clap {

/**
 * Parameter definition metadata used by ClapPluginShell to reflect
 * parameters to the CLAP host via the clap_plugin_params extension.
 */
struct ClapParamDef {
    clap_id id{0};
    const char* name{""};
    const char* module{""};
    double minValue{0.0};
    double maxValue{1.0};
    double defaultValue{0.0};
    clap_param_info_flags flags{CLAP_PARAM_IS_AUTOMATABLE};
    bool isMacro{false};
    uint32_t macroOrder{0};
};

/**
 * Zero-overhead, header-only C++ template adapter bridging the official
 * CLAP C-ABI (clap_plugin_t) to any modular Eatsbits DSP core or transpiled script.
 *
 * Guarantees:
 * 1. Hard Real-Time: Zero dynamic memory allocation during process() or flush().
 * 2. Sample-Accurate: Segments processing blocks at event timestamps for sample-exact
 *    parameter automation sweeps and note-on/note-off transitions.
 * 3. Planar Audio: Directly addresses 32-bit float left and right channel pointers.
 * 4. Dual Note Input: Handles both native CLAP note events and MIDI 1.0 fallback.
 * 5. State Persistence: Serializes parameter state with magic header and versioning.
 */
template <typename TDspAdapter>
class ClapPluginShell {
public:
    explicit ClapPluginShell(const clap_host_t* host) noexcept
        : host_(host) {
        initClapPluginStruct();
    }

    virtual ~ClapPluginShell() = default;

    // Non-copyable, non-movable
    ClapPluginShell(const ClapPluginShell&) = delete;
    ClapPluginShell& operator=(const ClapPluginShell&) = delete;
    ClapPluginShell(ClapPluginShell&&) = delete;
    ClapPluginShell& operator=(ClapPluginShell&&) = delete;

    [[nodiscard]] const clap_plugin_t* clapPlugin() const noexcept {
        return &plugin_;
    }

    [[nodiscard]] TDspAdapter& dsp() noexcept { return dsp_; }
    [[nodiscard]] const TDspAdapter& dsp() const noexcept { return dsp_; }
    [[nodiscard]] const clap_host_t* host() const noexcept { return host_; }

    [[nodiscard]] static ClapPluginShell* fromPlugin(const clap_plugin_t* plugin) noexcept {
        return static_cast<ClapPluginShell*>(plugin->plugin_data);
    }

    // --- Lifecycle Methods ---

    bool init() noexcept {
        return true;
    }

    bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) noexcept {
        sampleRate_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
        minFrames_ = minFrames;
        maxFrames_ = maxFrames;

        dsp_.setSampleRate(sampleRate_);
        dsp_.reset();
        active_ = true;
        return true;
    }

    void deactivate() noexcept {
        active_ = false;
        processing_ = false;
    }

    bool startProcessing() noexcept {
        processing_ = true;
        return true;
    }

    void stopProcessing() noexcept {
        processing_ = false;
    }

    void reset() noexcept {
        dsp_.reset();
    }

    // --- Audio & Event Processing ---

    clap_process_status process(const clap_process_t* proc) noexcept {
        if (!proc) return CLAP_PROCESS_ERROR;

        const uint32_t totalFrames = proc->frames_count;
        if (totalFrames == 0) return CLAP_PROCESS_CONTINUE;

        // Resolve planar stereo output buffers
        float* outL = nullptr;
        float* outR = nullptr;
        if (proc->audio_outputs_count > 0 && proc->audio_outputs) {
            const auto& outBuf = proc->audio_outputs[0];
            if (outBuf.data32 && outBuf.channel_count > 0) {
                outL = outBuf.data32[0];
                outR = (outBuf.channel_count > 1) ? outBuf.data32[1] : outL;
            }
        }

        const clap_input_events_t* inEvents = proc->in_events;
        const uint32_t numEvents = inEvents ? inEvents->size(inEvents) : 0;

        uint32_t currentFrame = 0;
        uint32_t eventIndex = 0;

        // Sample-accurate block slicing loop
        while (currentFrame < totalFrames) {
            // 1. Dispatch all events scheduled at or before the current frame
            while (eventIndex < numEvents) {
                const clap_event_header_t* hdr = inEvents->get(inEvents, eventIndex);
                if (!hdr || hdr->time > currentFrame) {
                    break;
                }
                dispatchEvent(hdr);
                ++eventIndex;
            }

            // 2. Determine boundary to next event
            uint32_t nextBoundary = totalFrames;
            if (eventIndex < numEvents) {
                const clap_event_header_t* nextHdr = inEvents->get(inEvents, eventIndex);
                if (nextHdr && nextHdr->time < nextBoundary) {
                    nextBoundary = nextHdr->time;
                }
            }

            // 3. Process DSP chunk up to next boundary
            const uint32_t chunkFrames = nextBoundary - currentFrame;
            if (chunkFrames > 0) {
                if (outL) {
                    float* chunkL = outL + currentFrame;
                    float* chunkR = outR ? (outR + currentFrame) : chunkL;
                    dsp_.processBlock(chunkL, chunkR, chunkFrames);
                }
                currentFrame = nextBoundary;
            }
        }

        // 4. Dispatch any remaining events at or past totalFrames
        while (eventIndex < numEvents) {
            const clap_event_header_t* hdr = inEvents->get(inEvents, eventIndex);
            if (hdr) {
                dispatchEvent(hdr);
            }
            ++eventIndex;
        }

        if constexpr (requires { dsp_.isActive(); }) {
            return dsp_.isActive() ? CLAP_PROCESS_CONTINUE : CLAP_PROCESS_SLEEP;
        } else {
            return CLAP_PROCESS_CONTINUE;
        }
    }

    void onMainThread() noexcept {}

    // --- Extension Dispatcher ---

    [[nodiscard]] const void* getExtension(const char* id) const noexcept {
        if (!id) return nullptr;
        if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
            return &audioPortsExt_;
        }
        if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
            return &notePortsExt_;
        }
        if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
            return &paramsExt_;
        }
        if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
            return &stateExt_;
        }
        if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) {
            return &latencyExt_;
        }
        if (std::strcmp(id, CLAP_EXT_REMOTE_CONTROLS) == 0 ||
            std::strcmp(id, CLAP_EXT_REMOTE_CONTROLS_COMPAT) == 0) {
            return &remoteControlsExt_;
        }
        if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
            return &guiExt_;
        }
        return nullptr;
    }

    // --- Event Dispatching ---

    void dispatchEvent(const clap_event_header_t* hdr) noexcept {
        if (!hdr) return;

        if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID) {
            switch (hdr->type) {
                case CLAP_EVENT_NOTE_ON: {
                    const auto* ev = reinterpret_cast<const clap_event_note_t*>(hdr);
                    if (ev->key >= 0 && ev->key <= 127) {
                        bool isSlide = false;
                        if constexpr (requires { dsp_.isActive(); }) {
                            isSlide = dsp_.isActive();
                        }
                        const bool isAccent = (ev->velocity > 0.75);
                        if constexpr (requires { dsp_.noteOn(static_cast<uint8_t>(ev->key), static_cast<float>(ev->velocity), isSlide, isAccent); }) {
                            dsp_.noteOn(static_cast<uint8_t>(ev->key), static_cast<float>(ev->velocity), isSlide, isAccent);
                        } else if constexpr (requires { dsp_.noteOn(static_cast<uint8_t>(ev->key), static_cast<float>(ev->velocity)); }) {
                            dsp_.noteOn(static_cast<uint8_t>(ev->key), static_cast<float>(ev->velocity));
                        }
                    }
                    break;
                }
                case CLAP_EVENT_NOTE_OFF:
                case CLAP_EVENT_NOTE_CHOKE: {
                    const auto* ev = reinterpret_cast<const clap_event_note_t*>(hdr);
                    if constexpr (requires { dsp_.noteOff(static_cast<uint8_t>(ev->key), static_cast<float>(ev->velocity)); }) {
                        dsp_.noteOff(static_cast<uint8_t>(ev->key), static_cast<float>(ev->velocity));
                    } else if constexpr (requires { dsp_.noteOff(); }) {
                        dsp_.noteOff();
                    }
                    break;
                }
                case CLAP_EVENT_PARAM_VALUE: {
                    const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
                    dsp_.setParamValue(ev->param_id, ev->value);
                    break;
                }
                case CLAP_EVENT_MIDI: {
                    const auto* ev = reinterpret_cast<const clap_event_midi_t*>(hdr);
                    handleMidiEvent(ev->data[0], ev->data[1], ev->data[2]);
                    break;
                }
                default:
                    break;
            }
        }
    }

    void handleMidiEvent(uint8_t status, uint8_t data1, uint8_t data2) noexcept {
        const uint8_t cmd = status & 0xF0;
        if (cmd == 0x90 && data2 > 0) { // Note On
            const float vel = static_cast<float>(data2) / 127.0f;
            bool isSlide = false;
            if constexpr (requires { dsp_.isActive(); }) {
                isSlide = dsp_.isActive();
            }
            const bool isAccent = (vel > 0.75f);
            if constexpr (requires { dsp_.noteOn(data1, vel, isSlide, isAccent); }) {
                dsp_.noteOn(data1, vel, isSlide, isAccent);
            } else if constexpr (requires { dsp_.noteOn(data1, vel); }) {
                dsp_.noteOn(data1, vel);
            }
        } else if (cmd == 0x80 || (cmd == 0x90 && data2 == 0)) { // Note Off
            if constexpr (requires { dsp_.noteOff(data1, static_cast<float>(data2) / 127.0f); }) {
                dsp_.noteOff(data1, static_cast<float>(data2) / 127.0f);
            } else if constexpr (requires { dsp_.noteOff(); }) {
                dsp_.noteOff();
            }
        } else if (cmd == 0xB0) { // Control Change
            if (data1 == 120 || data1 == 123) { // All Sound Off / All Notes Off
                dsp_.reset();
            }
        }
    }

    // --- State Persistence ---

    bool saveState(const clap_ostream_t* stream) const noexcept {
        if (!stream) return false;
        constexpr uint32_t magic = 0x53544145; // 'EATS'
        constexpr uint32_t version = 1;
        const uint32_t paramCount = dsp_.getParamCount();

        if (!writeExact(stream, &magic, sizeof(magic))) return false;
        if (!writeExact(stream, &version, sizeof(version))) return false;
        if (!writeExact(stream, &paramCount, sizeof(paramCount))) return false;

        for (uint32_t i = 0; i < paramCount; ++i) {
            const ClapParamDef& def = dsp_.getParamDef(i);
            const clap_id id = def.id;
            const double val = dsp_.getParamValue(id);
            if (!writeExact(stream, &id, sizeof(id))) return false;
            if (!writeExact(stream, &val, sizeof(val))) return false;
        }
        return true;
    }

    bool loadState(const clap_istream_t* stream) noexcept {
        if (!stream) return false;
        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t paramCount = 0;

        if (!readExact(stream, &magic, sizeof(magic)) || magic != 0x53544145) return false;
        if (!readExact(stream, &version, sizeof(version)) || version != 1) return false;
        if (!readExact(stream, &paramCount, sizeof(paramCount))) return false;

        for (uint32_t i = 0; i < paramCount; ++i) {
            clap_id id = 0;
            double val = 0.0;
            if (!readExact(stream, &id, sizeof(id))) return false;
            if (!readExact(stream, &val, sizeof(val))) return false;
            dsp_.setParamValue(id, val);
        }
        return true;
    }

private:
    static bool writeExact(const clap_ostream_t* stream, const void* buffer, uint64_t size) noexcept {
        const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
        uint64_t total = 0;
        while (total < size) {
            const int64_t w = stream->write(stream, ptr + total, size - total);
            if (w <= 0) return false;
            total += static_cast<uint64_t>(w);
        }
        return true;
    }

    static bool readExact(const clap_istream_t* stream, void* buffer, uint64_t size) noexcept {
        uint8_t* ptr = static_cast<uint8_t*>(buffer);
        uint64_t total = 0;
        while (total < size) {
            const int64_t r = stream->read(stream, ptr + total, size - total);
            if (r <= 0) return false;
            total += static_cast<uint64_t>(r);
        }
        return true;
    }

    void initClapPluginStruct() noexcept {
        plugin_.desc = TDspAdapter::getDescriptor();
        plugin_.plugin_data = this;

        plugin_.init = [](const clap_plugin* p) -> bool {
            return fromPlugin(p)->init();
        };
        plugin_.destroy = [](const clap_plugin* p) {
            delete fromPlugin(p);
        };
        plugin_.activate = [](const clap_plugin* p, double sr, uint32_t minF, uint32_t maxF) -> bool {
            return fromPlugin(p)->activate(sr, minF, maxF);
        };
        plugin_.deactivate = [](const clap_plugin* p) {
            fromPlugin(p)->deactivate();
        };
        plugin_.start_processing = [](const clap_plugin* p) -> bool {
            return fromPlugin(p)->startProcessing();
        };
        plugin_.stop_processing = [](const clap_plugin* p) {
            fromPlugin(p)->stopProcessing();
        };
        plugin_.reset = [](const clap_plugin* p) {
            fromPlugin(p)->reset();
        };
        plugin_.process = [](const clap_plugin* p, const clap_process_t* proc) -> clap_process_status {
            return fromPlugin(p)->process(proc);
        };
        plugin_.get_extension = [](const clap_plugin* p, const char* id) -> const void* {
            return fromPlugin(p)->getExtension(id);
        };
        plugin_.on_main_thread = [](const clap_plugin* p) {
            fromPlugin(p)->onMainThread();
        };

        // Extensions
        audioPortsExt_.count = [](const clap_plugin* /*p*/, bool is_input) -> uint32_t {
            return is_input ? 0 : 1;
        };
        audioPortsExt_.get = [](const clap_plugin* /*p*/, uint32_t index, bool is_input, clap_audio_port_info_t* info) -> bool {
            if (!info || is_input || index != 0) return false;
            info->id = 0;
            std::strncpy(info->name, "Main Output", CLAP_NAME_SIZE - 1);
            info->name[CLAP_NAME_SIZE - 1] = '\0';
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->channel_count = 2;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;
            return true;
        };

        notePortsExt_.count = [](const clap_plugin* /*p*/, bool is_input) -> uint32_t {
            return is_input ? 1 : 0;
        };
        notePortsExt_.get = [](const clap_plugin* /*p*/, uint32_t index, bool is_input, clap_note_port_info_t* info) -> bool {
            if (!info || !is_input || index != 0) return false;
            info->id = 0;
            std::strncpy(info->name, "Note Input", CLAP_NAME_SIZE - 1);
            info->name[CLAP_NAME_SIZE - 1] = '\0';
            info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
            info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
            return true;
        };

        paramsExt_.count = [](const clap_plugin* p) -> uint32_t {
            return fromPlugin(p)->dsp().getParamCount();
        };
        paramsExt_.get_info = [](const clap_plugin* p, uint32_t index, clap_param_info_t* info) -> bool {
            if (!info || index >= fromPlugin(p)->dsp().getParamCount()) return false;
            const ClapParamDef& def = fromPlugin(p)->dsp().getParamDef(index);
            info->id = def.id;
            info->flags = def.flags;
            info->cookie = nullptr;
            std::strncpy(info->name, def.name, CLAP_NAME_SIZE - 1);
            info->name[CLAP_NAME_SIZE - 1] = '\0';
            std::strncpy(info->module, def.module, CLAP_PATH_SIZE - 1);
            info->module[CLAP_PATH_SIZE - 1] = '\0';
            info->min_value = def.minValue;
            info->max_value = def.maxValue;
            info->default_value = def.defaultValue;
            return true;
        };
        paramsExt_.get_value = [](const clap_plugin* p, clap_id id, double* out) -> bool {
            if (!out) return false;
            *out = fromPlugin(p)->dsp().getParamValue(id);
            return true;
        };
        paramsExt_.value_to_text = [](const clap_plugin* p, clap_id id, double val, char* buf, uint32_t cap) -> bool {
            if (!buf || cap == 0) return false;
            if constexpr (requires { fromPlugin(p)->dsp().valueToText(id, val, buf, cap); }) {
                return fromPlugin(p)->dsp().valueToText(id, val, buf, cap);
            } else {
                std::snprintf(buf, cap, "%.2f", val);
                return true;
            }
        };
        paramsExt_.text_to_value = [](const clap_plugin* p, clap_id id, const char* text, double* out) -> bool {
            if (!text || !out) return false;
            if constexpr (requires { fromPlugin(p)->dsp().textToValue(id, text, out); }) {
                return fromPlugin(p)->dsp().textToValue(id, text, out);
            } else {
                char* end = nullptr;
                *out = std::strtod(text, &end);
                return (end != text);
            }
        };
        paramsExt_.flush = [](const clap_plugin* p, const clap_input_events_t* in, const clap_output_events_t* /*out*/) {
            if (!in) return;
            const uint32_t count = in->size(in);
            auto* self = fromPlugin(p);
            for (uint32_t i = 0; i < count; ++i) {
                const clap_event_header_t* hdr = in->get(in, i);
                if (!hdr) continue;
                if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID && hdr->type == CLAP_EVENT_PARAM_VALUE) {
                    const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
                    self->dsp().setParamValue(ev->param_id, ev->value);
                }
            }
        };

        stateExt_.save = [](const clap_plugin* p, const clap_ostream_t* stream) -> bool {
            return fromPlugin(p)->saveState(stream);
        };
        stateExt_.load = [](const clap_plugin* p, const clap_istream_t* stream) -> bool {
            return fromPlugin(p)->loadState(stream);
        };

        latencyExt_.get = [](const clap_plugin* /*p*/) -> uint32_t {
            return 0;
        };

        // Remote Controls extension (mapping macro parameters to hardware/DAW macro strips)
        remoteControlsExt_.count = [](const clap_plugin* /*p*/) -> uint32_t {
            return 1;
        };
        remoteControlsExt_.get = [](const clap_plugin* p, uint32_t page_index, clap_remote_controls_page_t* page) -> bool {
            if (!page || page_index != 0) return false;
            std::strncpy(page->section_name, "Main", CLAP_NAME_SIZE - 1);
            page->section_name[CLAP_NAME_SIZE - 1] = '\0';
            page->page_id = 0;
            std::strncpy(page->page_name, "Primary Macros", CLAP_NAME_SIZE - 1);
            page->page_name[CLAP_NAME_SIZE - 1] = '\0';
            page->is_for_preset = false;

            for (uint32_t i = 0; i < CLAP_REMOTE_CONTROLS_COUNT; ++i) {
                page->param_ids[i] = CLAP_INVALID_ID;
            }

            auto* self = fromPlugin(p);
            uint32_t slot = 0;
            uint32_t total = self->dsp().getParamCount();

            // First pass: add parameters flagged with isMacro == true
            for (uint32_t i = 0; i < total && slot < CLAP_REMOTE_CONTROLS_COUNT; ++i) {
                const auto& def = self->dsp().getParamDef(i);
                if (def.isMacro) {
                    page->param_ids[slot++] = def.id;
                }
            }
            // Second pass: fill any remaining slots with automatable parameters
            for (uint32_t i = 0; i < total && slot < CLAP_REMOTE_CONTROLS_COUNT; ++i) {
                const auto& def = self->dsp().getParamDef(i);
                if (!def.isMacro && (def.flags & CLAP_PARAM_IS_AUTOMATABLE)) {
                    page->param_ids[slot++] = def.id;
                }
            }
            return true;
        };

        // GUI Extension (Uniform vector canvas scaling)
        guiExt_.is_api_supported = [](const clap_plugin* /*p*/, const char* api, bool /*is_floating*/) -> bool {
            if (!api) return false;
#if defined(_WIN32)
            return std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
#elif defined(__APPLE__)
            return std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
#else
            return std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
#endif
        };
        guiExt_.get_preferred_api = [](const clap_plugin* /*p*/, const char** api, bool* is_floating) -> bool {
            if (!api || !is_floating) return false;
#if defined(_WIN32)
            *api = CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
            *api = CLAP_WINDOW_API_COCOA;
#else
            *api = CLAP_WINDOW_API_X11;
#endif
            *is_floating = false;
            return true;
        };
        guiExt_.create = [](const clap_plugin* /*p*/, const char* /*api*/, bool /*is_floating*/) -> bool {
            return true;
        };
        guiExt_.destroy = [](const clap_plugin* /*p*/) {};
        guiExt_.set_scale = [](const clap_plugin* /*p*/, double /*scale*/) -> bool {
            return true;
        };
        guiExt_.get_size = [](const clap_plugin* /*p*/, uint32_t* width, uint32_t* height) -> bool {
            if (!width || !height) return false;
            *width = 800;
            *height = 480;
            return true;
        };
        guiExt_.can_resize = [](const clap_plugin* /*p*/) -> bool {
            return true;
        };
        guiExt_.get_resize_hints = [](const clap_plugin* /*p*/, clap_gui_resize_hints_t* hints) -> bool {
            if (!hints) return false;
            hints->can_resize_horizontally = true;
            hints->can_resize_vertically = true;
            hints->preserve_aspect_ratio = true;
            hints->aspect_ratio_width = 800;
            hints->aspect_ratio_height = 480;
            return true;
        };
        guiExt_.adjust_size = [](const clap_plugin* /*p*/, uint32_t* width, uint32_t* height) -> bool {
            return (width && height);
        };
        guiExt_.set_size = [](const clap_plugin* /*p*/, uint32_t /*width*/, uint32_t /*height*/) -> bool {
            return true;
        };
        guiExt_.set_parent = [](const clap_plugin* /*p*/, const clap_window_t* /*window*/) -> bool {
            return true;
        };
        guiExt_.set_transient = [](const clap_plugin* /*p*/, const clap_window_t* /*window*/) -> bool {
            return true;
        };
        guiExt_.suggest_title = [](const clap_plugin* /*p*/, const char* /*title*/) {};
        guiExt_.show = [](const clap_plugin* /*p*/) -> bool { return true; };
        guiExt_.hide = [](const clap_plugin* /*p*/) -> bool { return true; };
    }

    clap_plugin_t plugin_{};
    const clap_host_t* host_{nullptr};
    TDspAdapter dsp_{};

    double sampleRate_{48000.0};
    uint32_t minFrames_{32};
    uint32_t maxFrames_{512};
    bool active_{false};
    bool processing_{false};

    clap_plugin_audio_ports_t audioPortsExt_{};
    clap_plugin_note_ports_t notePortsExt_{};
    clap_plugin_params_t paramsExt_{};
    clap_plugin_state_t stateExt_{};
    clap_plugin_latency_t latencyExt_{};
    clap_plugin_remote_controls_t remoteControlsExt_{};
    clap_plugin_gui_t guiExt_{};
};

} // namespace eatsbits::export_clap

/**
 * Macro helper generating the standard C-ABI entry point and factory.
 * Exported symbol: clap_entry (clap_plugin_entry_t)
 */
#define EATS_CLAP_EXPORT_PLUGIN(AdapterType)                                                      \
    static uint32_t CLAP_ABI eatsClapGetPluginCount(const clap_plugin_factory* /*factory*/) {     \
        return 1;                                                                                 \
    }                                                                                             \
    static const clap_plugin_descriptor_t* CLAP_ABI eatsClapGetPluginDesc(                        \
        const clap_plugin_factory* /*factory*/, uint32_t index) {                                 \
        if (index != 0) return nullptr;                                                           \
        return AdapterType::getDescriptor();                                                      \
    }                                                                                             \
    static const clap_plugin_t* CLAP_ABI eatsClapCreatePlugin(                                    \
        const clap_plugin_factory* /*factory*/, const clap_host_t* host, const char* plugin_id) { \
        if (!plugin_id || std::strcmp(plugin_id, AdapterType::getDescriptor()->id) != 0) {       \
            return nullptr;                                                                       \
        }                                                                                         \
        auto* shell = new (std::nothrow) ::eatsbits::export_clap::ClapPluginShell<AdapterType>(host); \
        return shell ? shell->clapPlugin() : nullptr;                                             \
    }                                                                                             \
    static const clap_plugin_factory_t s_eatsClapPluginFactory = {                                \
        .get_plugin_count = eatsClapGetPluginCount,                                               \
        .get_plugin_descriptor = eatsClapGetPluginDesc,                                           \
        .create_plugin = eatsClapCreatePlugin,                                                    \
    };                                                                                            \
    static bool CLAP_ABI eatsClapEntryInit(const char* /*plugin_path*/) {                         \
        return true;                                                                              \
    }                                                                                             \
    static void CLAP_ABI eatsClapEntryDeinit() {}                                                 \
    static const void* CLAP_ABI eatsClapEntryGetFactory(const char* factory_id) {                 \
        if (factory_id && std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {                 \
            return &s_eatsClapPluginFactory;                                                      \
        }                                                                                         \
        return nullptr;                                                                           \
    }                                                                                             \
    extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {                               \
        .clap_version = CLAP_VERSION,                                                             \
        .init = eatsClapEntryInit,                                                                \
        .deinit = eatsClapEntryDeinit,                                                            \
        .get_factory = eatsClapEntryGetFactory,                                                   \
    };

#endif // EATS_CLAP_PLUGIN_SHELL_HPP
