#include "eatsbits/lyrics/lyric_track.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <regex>
#include <iomanip>

namespace eatsbits::lyrics {

std::vector<LyricCue> LrcParser::parse(const std::string& lrcContent, float bpm) {
    std::vector<LyricCue> cues;
    if (bpm <= 0.0f) bpm = 120.0f;
    const float stepDurationSec = (60.0f / bpm) / 4.0f;
    if (stepDurationSec <= 0.0f) return cues;

    std::regex lrcTimestampRegex(R"(\[(\d{1,2}):(\d{2})(?:\.(\d{1,3}))?\])");
    std::regex enhancedWordRegex(R"(<(\d{1,2}):(\d{2})(?:\.(\d{1,3}))?>\s*([^<]+))");

    std::istringstream stream(lrcContent);
    std::string line;

    while (std::getline(stream, line)) {
        // Trim leading and trailing whitespace
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
            line.erase(line.begin());
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        // Skip metadata tags
        if (line.rfind("[ti:", 0) == 0 || line.rfind("[ar:", 0) == 0 ||
            line.rfind("[al:", 0) == 0 || line.rfind("[by:", 0) == 0 ||
            line.rfind("[offset:", 0) == 0) {
            continue;
        }

        std::smatch match;
        if (!std::regex_search(line, match, lrcTimestampRegex)) {
            continue;
        }

        int minutes = std::stoi(match[1].str());
        int seconds = std::stoi(match[2].str());
        float millis = 0.0f;
        if (match[3].matched) {
            std::string milStr = match[3].str();
            while (milStr.length() < 3) milStr.push_back('0');
            if (milStr.length() > 3) milStr = milStr.substr(0, 3);
            millis = static_cast<float>(std::stoi(milStr));
        }

        const float lineTimeSec = static_cast<float>(minutes * 60 + seconds) + (millis / 1000.0f);
        const float lineStartStep = lineTimeSec / stepDurationSec;

        std::string contentAfter = line.substr(match.position() + match.length());
        while (!contentAfter.empty() && (contentAfter.front() == ' ' || contentAfter.front() == '\t')) {
            contentAfter.erase(contentAfter.begin());
        }

        // Check for enhanced word-level tags
        std::sregex_iterator wordsBegin(contentAfter.begin(), contentAfter.end(), enhancedWordRegex);
        std::sregex_iterator wordsEnd;

        std::vector<std::pair<float, std::string>> wordTokens;
        for (auto it = wordsBegin; it != wordsEnd; ++it) {
            int wMin = std::stoi((*it)[1].str());
            int wSec = std::stoi((*it)[2].str());
            float wMil = 0.0f;
            if ((*it)[3].matched) {
                std::string wMilStr = (*it)[3].str();
                while (wMilStr.length() < 3) wMilStr.push_back('0');
                if (wMilStr.length() > 3) wMilStr = wMilStr.substr(0, 3);
                wMil = static_cast<float>(std::stoi(wMilStr));
            }
            float wTimeSec = static_cast<float>(wMin * 60 + wSec) + (wMil / 1000.0f);
            float wStep = wTimeSec / stepDurationSec;
            std::string wText = (*it)[4].str();
            while (!wText.empty() && (wText.back() == ' ' || wText.back() == '\t')) wText.pop_back();
            if (!wText.empty()) {
                wordTokens.emplace_back(wStep, wText);
            }
        }

        if (!wordTokens.empty()) {
            for (size_t i = 0; i < wordTokens.size(); ++i) {
                float dur = 1.0f;
                if (i + 1 < wordTokens.size()) {
                    dur = std::clamp(wordTokens[i + 1].first - wordTokens[i].first, 0.5f, 16.0f);
                }
                LyricCue cue;
                cue.id = "cue_" + std::to_string(cues.size()) + "_" + std::to_string(static_cast<int>(wordTokens[i].first));
                cue.startStep = wordTokens[i].first;
                cue.durationSteps = dur;
                cue.text = wordTokens[i].second;
                cues.push_back(cue);
            }
        } else if (!contentAfter.empty()) {
            LyricCue cue;
            cue.id = "cue_" + std::to_string(cues.size()) + "_" + std::to_string(static_cast<int>(lineStartStep));
            cue.startStep = lineStartStep;
            cue.durationSteps = 4.0f; // 1 beat default
            cue.text = contentAfter;
            cues.push_back(cue);
        }
    }

    std::sort(cues.begin(), cues.end(), [](const LyricCue& a, const LyricCue& b) {
        return a.startStep < b.startStep;
    });

    return cues;
}

std::string LrcParser::exportToLrc(
    const std::vector<LyricCue>& cues,
    float bpm,
    const std::string& title,
    const std::string& artist
) {
    if (bpm <= 0.0f) bpm = 120.0f;
    const float stepDurationSec = (60.0f / bpm) / 4.0f;

    std::ostringstream out;
    out << "[ti:" << title << "]\n";
    out << "[ar:" << artist << "]\n";
    out << "[by:Eatsbits Modular DAW]\n";

    for (const auto& cue : cues) {
        float totalSec = cue.startStep * stepDurationSec;
        int minutes = static_cast<int>(totalSec) / 60;
        int seconds = static_cast<int>(totalSec) % 60;
        int hundredths = static_cast<int>((totalSec - std::floor(totalSec)) * 100.0f);

        out << "["
            << std::setw(2) << std::setfill('0') << minutes << ":"
            << std::setw(2) << std::setfill('0') << seconds << "."
            << std::setw(2) << std::setfill('0') << hundredths << "] "
            << cue.text << "\n";
    }

    return out.str();
}

void LyricTrack::addCue(LyricCue cue) {
    if (cue.id.empty()) {
        cue.id = "cue_" + std::to_string(cues_.size()) + "_" + std::to_string(static_cast<int>(cue.startStep * 10.0f));
    }
    cues_.push_back(std::move(cue));
    std::sort(cues_.begin(), cues_.end(), [](const LyricCue& a, const LyricCue& b) {
        return a.startStep < b.startStep;
    });
}

bool LyricTrack::removeCue(const std::string& cueId) {
    auto it = std::remove_if(cues_.begin(), cues_.end(), [&](const LyricCue& c) {
        return c.id == cueId;
    });
    if (it != cues_.end()) {
        cues_.erase(it, cues_.end());
        return true;
    }
    return false;
}

const LyricCue* LyricTrack::getCueAtStep(float step) const noexcept {
    for (const auto& cue : cues_) {
        if (step >= cue.startStep && step < (cue.startStep + cue.durationSteps)) {
            return &cue;
        }
    }
    return nullptr;
}

std::vector<LyricCue> LyricTrack::getCuesInRange(float startStep, float endStep) const {
    std::vector<LyricCue> result;
    for (const auto& cue : cues_) {
        float cueEnd = cue.startStep + cue.durationSteps;
        if (cueEnd > startStep && cue.startStep < endStep) {
            result.push_back(cue);
        }
    }
    return result;
}

json::Value LyricTrack::toJson() const {
    json::Object obj;
    obj["id"] = id_;
    obj["name"] = name_;
    json::Array cuesArr;
    for (const auto& cue : cues_) {
        cuesArr.push_back(cue.toJson());
    }
    obj["cues"] = json::Value{std::move(cuesArr)};
    return json::Value{std::move(obj)};
}

LyricTrack LyricTrack::fromJson(const json::Value& val) {
    LyricTrack track;
    if (!val.isObject()) return track;
    track.id_ = val["id"].asString();
    track.name_ = val["name"].asString();
    if (val.contains("cues") && val["cues"].isArray()) {
        for (const auto& cueVal : val["cues"].asArray()) {
            track.cues_.push_back(LyricCue::fromJson(cueVal));
        }
        std::sort(track.cues_.begin(), track.cues_.end(), [](const LyricCue& a, const LyricCue& b) {
            return a.startStep < b.startStep;
        });
    }
    return track;
}

} // namespace eatsbits::lyrics
