#ifndef EATS_AI_ASSISTANT_DIALOG_HPP
#define EATS_AI_ASSISTANT_DIALOG_HPP

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "../../ai/gemini_client.hpp"
#include "../../ai/ai_mixing_engine.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

enum class AiDialogTab {
    Compose = 0,
    SoundDesign = 1,
    AutoMix = 2,
    Settings = 3
};

/**
 * AiAssistantDialog: Natural language AI copilot modal for song generation,
 * procedural EatScript synthesis, algorithmic auto-mixing, and Gemini API configuration.
 */
class AiAssistantDialog {
public:
    AiAssistantDialog();
    ~AiAssistantDialog() = default;

    void open(AiDialogTab initialTab = AiDialogTab::AutoMix);
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);
    bool handleChar(unsigned int codepoint);

    void setActiveTab(AiDialogTab tab) noexcept { activeTab_ = tab; }
    [[nodiscard]] AiDialogTab getActiveTab() const noexcept { return activeTab_; }

    [[nodiscard]] ai::GeminiClient& getClient() noexcept { return client_; }
    [[nodiscard]] const ai::GeminiClient& getClient() const noexcept { return client_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }

    // Interactivity callbacks
    std::function<void(const std::string& code, const std::string& category)> onCodeInjected;
    std::function<void(const ai::AiMixResult& mixResult)> onMixCompleted;
    std::function<void(const std::string& songBlueprintJson)> onSongBlueprintGenerated;

    // Direct invocation helper for Sequencer integration
    void executeAutoMix(sequencer::StepSequencer& seq);

private:
    void renderTabs(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderComposeTab(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderSoundDesignTab(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderAutoMixTab(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderSettingsTab(BatchRenderer2D& r, const ThemeTokens& theme);

    bool isOpen_{false};
    AiDialogTab activeTab_{AiDialogTab::AutoMix};

    ai::GeminiClient client_{};

    // State per tab
    // Tab 0: Compose
    std::string composePrompt_{"Cyberpunk Synthwave chase scene in D minor with 303 acid line"};
    std::string generatedBlueprint_{""};

    // Tab 1: Sound Design
    int soundCategory_{0}; // 0 = Synth, 1 = Audio FX, 2 = MIDI FX
    std::string soundPrompt_{"Warm analog pad with chorus and subtle pitch drift"};
    std::string generatedCode_{""};

    // Tab 2: Auto-Mix
    std::string selectedGenre_{"Synthwave"};
    float selectedTargetLufs_{-14.0f};
    std::string mixInstructions_{""};
    std::string mixSummary_{"Ready to analyze and auto-mix master."};
    int lastTracksAdjusted_{0};

    // Tab 3: Settings
    std::string apiKeyInput_{""};
    std::string connectionStatus_{"Ready"};
    bool isConnected_{false};

    // Text field focus tracking
    int focusedInput_{-1}; // -1 = none, 0 = compose, 1 = sound prompt, 2 = mix inst, 3 = api key

    // Bounding boxes
    Rect2D dialogBounds_{};
    std::vector<Rect2D> tabBounds_{};

    // Tab 0 buttons
    Rect2D composeInputBounds_{};
    Rect2D generateSongBtnBounds_{};

    // Tab 1 buttons
    std::vector<Rect2D> soundCategoryPillBounds_{};
    Rect2D soundInputBounds_{};
    Rect2D generateSoundBtnBounds_{};
    Rect2D injectCodeBtnBounds_{};
    Rect2D codePreviewBounds_{};

    // Tab 2 buttons
    std::vector<Rect2D> genrePillBounds_{};
    std::vector<Rect2D> lufsPillBounds_{};
    Rect2D runAutoMixBtnBounds_{};
    Rect2D mixSummaryBounds_{};

    // Tab 3 buttons
    Rect2D apiKeyInputBounds_{};
    Rect2D testConnBtnBounds_{};
    Rect2D modelToggleBounds_{};

    // Close button
    Rect2D closeBtnBounds_{};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
};

} // namespace eatsbits::ui

#endif // EATS_AI_ASSISTANT_DIALOG_HPP
