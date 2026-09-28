#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include "eatsbits/project/json_parser.hpp"

namespace eatsbits::lyrics {

/**
 * @brief Represents a single time-synchronized lyric syllable, word, or phrase event.
 */
struct LyricCue {
    std::string id;
    float startStep{0.0f};         // Musical time in 16th steps relative to track/clip start
    float durationSteps{1.0f};    // Duration in 16th steps (default 1.0 = one 16th step)
    std::string text;             // Syllable or word text (e.g. "Wel-", "come", "to")
    std::string phoneticOverride; // Optional SSML/IPA/ARPAbet phonetic override for speech synthesis
    float pitch{1.0f};            // Relative pitch multiplier (0.5 to 2.0, default 1.0)
    float rate{1.0f};             // Relative speech rate multiplier (0.5 to 2.0, default 1.0)

    [[nodiscard]] json::Value toJson() const {
        json::Object obj;
        obj["id"] = id;
        obj["startStep"] = static_cast<double>(startStep);
        obj["durationSteps"] = static_cast<double>(durationSteps);
        obj["text"] = text;
        if (!phoneticOverride.empty()) {
            obj["phoneticOverride"] = phoneticOverride;
        }
        obj["pitch"] = static_cast<double>(pitch);
        obj["rate"] = static_cast<double>(rate);
        return json::Value{std::move(obj)};
    }

    [[nodiscard]] static LyricCue fromJson(const json::Value& val) {
        LyricCue cue;
        if (!val.isObject()) return cue;
        cue.id = val["id"].asString();
        cue.startStep = static_cast<float>(val["startStep"].asDouble());
        cue.durationSteps = static_cast<float>(val["durationSteps"].asDouble());
        if (cue.durationSteps <= 0.0f) cue.durationSteps = 1.0f;
        cue.text = val["text"].asString();
        if (val.contains("phoneticOverride")) {
            cue.phoneticOverride = val["phoneticOverride"].asString();
        }
        if (val.contains("pitch")) {
            cue.pitch = static_cast<float>(val["pitch"].asDouble());
        }
        if (val.contains("rate")) {
            cue.rate = static_cast<float>(val["rate"].asDouble());
        }
        return cue;
    }
};

/**
 * @brief High-precision LRC (Standard & Enhanced word-level) parser and exporter.
 */
class LrcParser {
public:
    /**
     * @brief Parses an LRC string into a list of LyricCues mapped to 16th-note steps.
     * @param lrcContent The raw LRC file text.
     * @param bpm Project tempo in beats per minute.
     * @return Sorted vector of LyricCue events.
     */
    [[nodiscard]] static std::vector<LyricCue> parse(const std::string& lrcContent, float bpm = 120.0f);

    /**
     * @brief Exports a list of LyricCues to standard LRC string format at the given tempo.
     * @param cues List of synchronized lyric cues.
     * @param bpm Project tempo in beats per minute.
     * @param title Track/song title header.
     * @param artist Artist name header.
     * @return Formatted LRC text.
     */
    [[nodiscard]] static std::string exportToLrc(
        const std::vector<LyricCue>& cues,
        float bpm = 120.0f,
        const std::string& title = "Untitled",
        const std::string& artist = "Eatsbits"
    );
};

/**
 * @brief Timeline container for lyrics associated with a track or the global arrangement.
 */
class LyricTrack {
public:
    LyricTrack(std::string id = "lyrics_main", std::string name = "Lyrics")
        : id_(std::move(id)), name_(std::move(name)) {}

    [[nodiscard]] const std::string& getId() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::vector<LyricCue>& getCues() const noexcept { return cues_; }
    [[nodiscard]] std::vector<LyricCue>& getCues() noexcept { return cues_; }

    void addCue(LyricCue cue);
    bool removeCue(const std::string& cueId);
    void clear() noexcept { cues_.clear(); }

    [[nodiscard]] const LyricCue* getCueAtStep(float step) const noexcept;
    [[nodiscard]] std::vector<LyricCue> getCuesInRange(float startStep, float endStep) const;

    [[nodiscard]] json::Value toJson() const;
    static LyricTrack fromJson(const json::Value& val);

private:
    std::string id_;
    std::string name_;
    std::vector<LyricCue> cues_;
};

} // namespace eatsbits::lyrics
