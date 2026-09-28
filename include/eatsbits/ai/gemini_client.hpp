#ifndef EATS_GEMINI_CLIENT_HPP
#define EATS_GEMINI_CLIENT_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include "../project/json_parser.hpp"

namespace eatsbits::ai {

enum class GeminiKeySource {
    None,
    SessionOnly,
    SettingsFile,
    EnvironmentVariable
};

struct GeminiConfig {
    std::string apiKey{""};
    std::string model{"gemini-2.5-flash"};
    std::string baseUrl{"https://generativelanguage.googleapis.com/v1beta/models"};
    uint32_t timeoutMs{15000};
    bool offlineMockMode{false}; // Always returns deterministic high-quality responses without internet
};

struct GeminiResponse {
    bool success{false};
    int statusCode{0};
    std::string text{""};
    std::string rawJson{""};
    std::string errorMessage{""};
};

/**
 * Native Google Gemini API client bridge with Bring-Your-Own-Key (BYOK) support,
 * automated prompt chaining, and built-in offline audio-intelligence fallback.
 */
class GeminiClient {
public:
    explicit GeminiClient(GeminiConfig config = {});
    ~GeminiClient() = default;

    void setConfig(const GeminiConfig& config) noexcept { config_ = config; }
    [[nodiscard]] const GeminiConfig& getConfig() const noexcept { return config_; }

    void setApiKey(const std::string& key) noexcept;
    [[nodiscard]] const std::string& getApiKey() const noexcept { return config_.apiKey; }
    [[nodiscard]] bool hasApiKey() const noexcept { return !config_.apiKey.empty(); }

    void setModel(const std::string& model) noexcept { config_.model = model; }
    [[nodiscard]] const std::string& getModel() const noexcept { return config_.model; }

    void setOfflineMock(bool enabled) noexcept { config_.offlineMockMode = enabled; }
    [[nodiscard]] bool isOfflineMock() const noexcept { return config_.offlineMockMode; }

    // Synchronous execution
    [[nodiscard]] GeminiResponse generateContent(
        const std::string& prompt,
        const std::string& systemInstruction = "",
        float temperature = 0.7f,
        int maxTokens = 2048
    );

    [[nodiscard]] GeminiResponse testConnection();

    // High-level creative helpers
    [[nodiscard]] std::string generateEatscript(const std::string& soundPrompt, const std::string& category = "instrument");
    [[nodiscard]] std::string generateSongBlueprint(const std::string& genrePrompt);

    // Asynchronous execution with callback
    void generateContentAsync(
        const std::string& prompt,
        const std::string& systemInstruction,
        std::function<void(const GeminiResponse&)> onComplete,
        float temperature = 0.7f,
        int maxTokens = 2048
    );

    static std::string getFallbackEatscript(const std::string& soundPrompt, const std::string& category);
    static std::string getFallbackMixPatch(const std::string& genre, float targetLufs);

private:
    GeminiConfig config_{};
    GeminiResponse executeHttpRequest(const std::string& url, const std::string& jsonBody);
};

} // namespace eatsbits::ai

#endif // EATS_GEMINI_CLIENT_HPP
