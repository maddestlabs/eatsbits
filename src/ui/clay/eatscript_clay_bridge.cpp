#include "eatsbits/ui/clay/eatscript_clay_bridge.hpp"
#include <mutex>
#include <iostream>
#include <sstream>

namespace eatsbits::ui::clay {

namespace {
    UiLayoutEngine* s_activeEngine = nullptr;
    std::mutex s_engineMutex;

    float getFloatArg(const std::vector<EatsAbiValue>& args, size_t idx, float defaultVal) {
        if (idx >= args.size()) return defaultVal;
        const auto& a = args[idx];
        if (a.type == EATS_ABI_TYPE_F32) return a.as.f32;
        if (a.type == EATS_ABI_TYPE_F64) return static_cast<float>(a.as.f64);
        if (a.type == EATS_ABI_TYPE_I32) return static_cast<float>(a.as.i32);
        if (a.type == EATS_ABI_TYPE_I64) return static_cast<float>(a.as.i64);
        return defaultVal;
    }

    std::string getStringArg(const std::vector<EatsAbiValue>& args, size_t idx, const std::string& defaultVal = "") {
        if (idx >= args.size()) return defaultVal;
        const auto& a = args[idx];
        if (a.type == EATS_ABI_TYPE_STRING && a.as.stringVal) return a.as.stringVal;
        return defaultVal;
    }

    bool getBoolArg(const std::vector<EatsAbiValue>& args, size_t idx, bool defaultVal = false) {
        if (idx >= args.size()) return defaultVal;
        const auto& a = args[idx];
        if (a.type == EATS_ABI_TYPE_BOOL) return a.as.boolean != 0;
        if (a.type == EATS_ABI_TYPE_I32) return a.as.i32 != 0;
        return defaultVal;
    }

    UiLayoutEngine& getEngineOrFallback(UiLayoutEngine* explicitEngine) {
        if (explicitEngine) return *explicitEngine;
        if (s_activeEngine) return *s_activeEngine;
        thread_local static UiLayoutEngine s_threadEngine;
        return s_threadEngine;
    }

    uint32_t parseHexColor(std::string_view hex, uint32_t defaultCol = 0xFFFFFFFF) {
        if (hex.empty()) return defaultCol;
        if (hex.front() == '#') hex.remove_prefix(1);
        try {
            if (hex.size() == 6) {
                uint32_t rgb = static_cast<uint32_t>(std::stoul(std::string(hex), nullptr, 16));
                return 0xFF000000 | rgb;
            } else if (hex.size() == 8) {
                return static_cast<uint32_t>(std::stoul(std::string(hex), nullptr, 16));
            }
        } catch (...) {}
        return defaultCol;
    }
} // namespace

void setActiveUiLayoutEngine(UiLayoutEngine* engine) {
    std::lock_guard<std::mutex> lock(s_engineMutex);
    s_activeEngine = engine;
}

UiLayoutEngine* getActiveUiLayoutEngine() {
    std::lock_guard<std::mutex> lock(s_engineMutex);
    return s_activeEngine;
}

void registerClayLayoutHostFunctions(abi::HostRegistry& registry,
                                     UiLayoutEngine* explicitEngine,
                                     BatchRenderer2D* explicitRenderer) {
    // 1. ui_begin_column
    registry.registerHostFunction("ui_begin_column", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            if (args.size() >= 2) {
                float w = getFloatArg(args, 0, 1440.0f);
                float h = getFloatArg(args, 1, 900.0f);
                eng.beginRoot(0.0f, 0.0f, w, h, LayoutDirection::Column);
            } else if (args.size() == 1) {
                float h = getFloatArg(args, 0, 100.0f);
                eng.beginContainer(LayoutDirection::Column, LayoutConstraint::fixed(h));
            } else {
                eng.beginContainer(LayoutDirection::Column, LayoutConstraint::flex(1.0f));
            }
            return eats_abi_value_void();
        }, "Begin a vertical Column container: (w, h) for root or (h) for fixed height or () for flex");

    // 2. ui_begin_row
    registry.registerHostFunction("ui_begin_row", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            if (args.size() >= 2) {
                float w = getFloatArg(args, 0, 1440.0f);
                float h = getFloatArg(args, 1, 900.0f);
                eng.beginRoot(0.0f, 0.0f, w, h, LayoutDirection::Row);
            } else if (args.size() == 1) {
                float w = getFloatArg(args, 0, 100.0f);
                eng.beginContainer(LayoutDirection::Row, LayoutConstraint::fixed(w));
            } else {
                eng.beginContainer(LayoutDirection::Row, LayoutConstraint::flex(1.0f));
            }
            return eats_abi_value_void();
        }, "Begin a horizontal Row container: (w, h) for root or (w) for fixed width or () for flex");

    // 3. ui_begin_fixed_row
    registry.registerHostFunction("ui_begin_fixed_row", EATS_ABI_TYPE_VOID, {EATS_ABI_TYPE_F32},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            float h = getFloatArg(args, 0, 48.0f);
            eng.beginContainer(LayoutDirection::Row, LayoutConstraint::fixed(h));
            return eats_abi_value_void();
        }, "Begin a Row with fixed height in a Column: (height_px)");

    // 4. ui_begin_flex_row
    registry.registerHostFunction("ui_begin_flex_row", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            float weight = getFloatArg(args, 0, 1.0f);
            eng.beginContainer(LayoutDirection::Row, LayoutConstraint::flex(weight));
            return eats_abi_value_void();
        }, "Begin a Row with proportional flex height in a Column: (weight = 1.0)");

    // 5. ui_begin_fixed_column
    registry.registerHostFunction("ui_begin_fixed_column", EATS_ABI_TYPE_VOID, {EATS_ABI_TYPE_F32},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            float w = getFloatArg(args, 0, 200.0f);
            eng.beginContainer(LayoutDirection::Column, LayoutConstraint::fixed(w));
            return eats_abi_value_void();
        }, "Begin a Column with fixed width in a Row: (width_px)");

    // 6. ui_begin_flex_column
    registry.registerHostFunction("ui_begin_flex_column", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            float weight = getFloatArg(args, 0, 1.0f);
            eng.beginContainer(LayoutDirection::Column, LayoutConstraint::flex(weight));
            return eats_abi_value_void();
        }, "Begin a Column with proportional flex width in a Row: (weight = 1.0)");

    // 7. ui_begin_stack
    registry.registerHostFunction("ui_begin_stack", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>&) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            eng.beginContainer(LayoutDirection::Stack, LayoutConstraint::fill());
            return eats_abi_value_void();
        }, "Begin an overlaid Stack container (children share viewport in z-order)");

    // 8. ui_end
    registry.registerHostFunction("ui_end", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>&) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            eng.endContainer();
            return eats_abi_value_void();
        }, "End the active layout container (Column, Row, or Stack)");

    // 9. ui_end_container (alias)
    registry.registerHostFunction("ui_end_container", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>&) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            eng.endContainer();
            return eats_abi_value_void();
        }, "Alias for ui_end()");

    // 10. ui_spacer
    registry.registerHostFunction("ui_spacer", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            float sz = getFloatArg(args, 0, 8.0f);
            bool isFlex = getBoolArg(args, 1, false);
            eng.addSpacer(sz, isFlex);
            return eats_abi_value_void();
        }, "Insert a layout spacing gap: (size_px, is_flex=false)");

    // 11. ui_push_id
    registry.registerHostFunction("ui_push_id", EATS_ABI_TYPE_VOID, {EATS_ABI_TYPE_STRING},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string id = getStringArg(args, 0);
            if (!id.empty()) {
                eng.pushId(id);
            }
            return eats_abi_value_void();
        }, "Push a namespace ID onto the immediate-mode widget ID stack: (id)");

    // 12. ui_pop_id
    registry.registerHostFunction("ui_pop_id", EATS_ABI_TYPE_VOID, {},
        [explicitEngine](const std::vector<EatsAbiValue>&) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            eng.popId();
            return eats_abi_value_void();
        }, "Pop the topmost namespace ID from the widget ID stack");

    // 13. ui_get_id
    registry.registerHostFunction("ui_get_id", EATS_ABI_TYPE_STRING, {EATS_ABI_TYPE_STRING},
        [explicitEngine](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string raw = getStringArg(args, 0);
            thread_local static std::string s_lastScopedId;
            s_lastScopedId = eng.getScopedId(raw);
            return eats_abi_value_string(s_lastScopedId.c_str());
        }, "Get the full hierarchical scoped ID: (name)");

    // 14. ui_load_texture
    registry.registerHostFunction("ui_load_texture", EATS_ABI_TYPE_I32, {EATS_ABI_TYPE_STRING},
        [explicitRenderer](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            auto* r = explicitRenderer ? explicitRenderer : BatchRenderer2D::getActiveBatchRenderer();
            std::string uri = getStringArg(args, 0);
            uint32_t handle = (r && !uri.empty()) ? r->loadTextureFromUri(uri) : 0;
            return eats_abi_value_i32(static_cast<int32_t>(handle));
        }, "Load in-memory or Data URI texture: (uri) -> handle");
}

void registerEatscriptUiModule(eatscript::Evaluator& evaluator,
                               UiLayoutEngine* explicitEngine,
                               BatchRenderer2D* explicitRenderer) {
    evaluator.setVariable("ui", eatscript::Value(std::map<std::string, eatscript::Value>{}));

    // 1. ui.loadTexture(uri)
    evaluator.registerMemberFunction("ui", "loadTexture",
        [explicitRenderer](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            if (args.empty()) return eatscript::Value(0);
            std::string uri = args[0].asString();
            auto* r = explicitRenderer ? explicitRenderer : BatchRenderer2D::getActiveBatchRenderer();
            uint32_t handle = (r && !uri.empty()) ? r->loadTextureFromUri(uri) : 0;
            return eatscript::Value(static_cast<int>(handle));
        });

    // 2. ui.panel(opts, callback)
    evaluator.registerMemberFunction("ui", "panel",
        [&evaluator, explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            eatscript::Value optsVal;
            eatscript::Value cbVal;
            if (args.size() >= 2) {
                optsVal = args[0];
                cbVal = args[1];
            } else if (args.size() == 1) {
                if (args[0].isFunction()) {
                    cbVal = args[0];
                } else {
                    optsVal = args[0];
                }
            }

            float pad = optsVal.isDict() ? static_cast<float>(optsVal.get("padding", eatscript::Value(0.0)).asNumber()) : 0.0f;
            float gap = optsVal.isDict() ? static_cast<float>(optsVal.get("gap", eatscript::Value(0.0)).asNumber()) : 0.0f;

            eng.beginContainer(LayoutDirection::Column, LayoutConstraint::fill(), LayoutConstraint::fill(), gap, pad);

            if (cbVal.isFunction()) {
                evaluator.invokeCallable(cbVal);
            }

            eng.endContainer();
            return eatscript::Value();
        });

    // 3. ui.row(opts, callback)
    evaluator.registerMemberFunction("ui", "row",
        [&evaluator, explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            eatscript::Value optsVal;
            eatscript::Value cbVal;
            if (args.size() >= 2) {
                optsVal = args[0];
                cbVal = args[1];
            } else if (args.size() == 1) {
                if (args[0].isFunction()) {
                    cbVal = args[0];
                } else {
                    optsVal = args[0];
                }
            }

            float gap = optsVal.isDict() ? static_cast<float>(optsVal.get("gap", eatscript::Value(0.0)).asNumber()) : 0.0f;
            float pad = optsVal.isDict() ? static_cast<float>(optsVal.get("padding", eatscript::Value(0.0)).asNumber()) : 0.0f;
            bool grow = optsVal.isDict() && optsVal.get("grow", eatscript::Value(false)).asBoolean();
            float height = optsVal.isDict() ? static_cast<float>(optsVal.get("height", eatscript::Value(0.0)).asNumber()) : 0.0f;

            LayoutConstraint main = (height > 0.0f) ? LayoutConstraint::fixed(height) :
                                   (grow ? LayoutConstraint::flex(1.0f) : LayoutConstraint::flex(0.0f));
            eng.beginContainer(LayoutDirection::Row, main, LayoutConstraint::fill(), gap, pad);

            if (cbVal.isFunction()) {
                evaluator.invokeCallable(cbVal);
            }

            eng.endContainer();
            return eatscript::Value();
        });

    // 4. ui.column(opts, callback)
    evaluator.registerMemberFunction("ui", "column",
        [&evaluator, explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            eatscript::Value optsVal;
            eatscript::Value cbVal;
            if (args.size() >= 2) {
                optsVal = args[0];
                cbVal = args[1];
            } else if (args.size() == 1) {
                if (args[0].isFunction()) {
                    cbVal = args[0];
                } else {
                    optsVal = args[0];
                }
            }

            float gap = optsVal.isDict() ? static_cast<float>(optsVal.get("gap", eatscript::Value(0.0)).asNumber()) : 0.0f;
            float pad = optsVal.isDict() ? static_cast<float>(optsVal.get("padding", eatscript::Value(0.0)).asNumber()) : 0.0f;
            bool grow = optsVal.isDict() && optsVal.get("grow", eatscript::Value(false)).asBoolean();
            float width = optsVal.isDict() ? static_cast<float>(optsVal.get("width", eatscript::Value(0.0)).asNumber()) : 0.0f;

            LayoutConstraint main = (width > 0.0f) ? LayoutConstraint::fixed(width) :
                                   (grow ? LayoutConstraint::flex(1.0f) : LayoutConstraint::flex(0.0f));
            eng.beginContainer(LayoutDirection::Column, main, LayoutConstraint::fill(), gap, pad);

            if (cbVal.isFunction()) {
                evaluator.invokeCallable(cbVal);
            }

            eng.endContainer();
            return eatscript::Value();
        });

    // 5. ui.stack(opts, callback)
    evaluator.registerMemberFunction("ui", "stack",
        [&evaluator, explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            eatscript::Value cbVal = args.empty() ? eatscript::Value() : (args.size() == 1 ? args[0] : args[1]);

            eng.beginContainer(LayoutDirection::Stack, LayoutConstraint::fill(), LayoutConstraint::fill());

            if (cbVal.isFunction()) {
                evaluator.invokeCallable(cbVal);
            }

            eng.endContainer();
            return eatscript::Value();
        });

    // 6. ui.knob(id, value, opts) or ui.knob({ label: "CUTOFF", value: 0.75, ... })
    evaluator.registerMemberFunction("ui", "knob",
        [explicitEngine, explicitRenderer](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string id = "Knob";
            float val = 0.0f;
            float minVal = 0.0f;
            float maxVal = 1.0f;
            eatscript::Value opts;

            if (args.size() == 1 && args[0].isDict()) {
                opts = args[0];
                id = opts.get("label", opts.get("id", eatscript::Value("Knob"))).asString();
                val = static_cast<float>(opts.get("value", opts.get("default", eatscript::Value(0.0))).asNumber());
                minVal = static_cast<float>(opts.get("min", eatscript::Value(0.0)).asNumber());
                maxVal = static_cast<float>(opts.get("max", eatscript::Value(1.0)).asNumber());
            } else {
                if (args.size() > 0) id = args[0].asString();
                if (args.size() > 1) {
                    if (args[1].isDict()) {
                        opts = args[1];
                        val = static_cast<float>(opts.get("default", opts.get("value", eatscript::Value(0.0))).asNumber());
                        minVal = static_cast<float>(opts.get("min", eatscript::Value(0.0)).asNumber());
                        maxVal = static_cast<float>(opts.get("max", eatscript::Value(1.0)).asNumber());
                    } else {
                        val = static_cast<float>(args[1].asNumber());
                    }
                }
                if (args.size() > 2 && args[2].isDict()) {
                    opts = args[2];
                }
            }

            std::string style = opts.isDict() ? opts.get("style", eatscript::Value("studio")).asString() : "studio";
            std::string accentStr = opts.isDict() ? opts.get("accent", eatscript::Value("#ff8c00")).asString() : "#ff8c00";
            uint32_t accentCol = parseHexColor(accentStr, 0xFFFF8C00);
            float radius = opts.isDict() ? static_cast<float>(opts.get("radius", opts.get("size", eatscript::Value(24.0))).asNumber()) : 24.0f;

            uint32_t filmstripTex = 0;
            int frames = 0;
            bool isHorizontal = false;
            auto getTexKey = [](const eatscript::Value& d) -> std::optional<eatscript::Value> {
                if (d.hasKey("filmstrip")) return d.get("filmstrip");
                if (d.hasKey("texture")) return d.get("texture");
                return std::nullopt;
            };
            if (opts.isDict()) {
                auto fsOpt = getTexKey(opts);
                if (fsOpt.has_value()) {
                    auto fsVal = *fsOpt;
                    if (fsVal.isNumber()) {
                        filmstripTex = static_cast<uint32_t>(fsVal.asNumber());
                    } else if (fsVal.isString()) {
                        auto* r = explicitRenderer ? explicitRenderer : BatchRenderer2D::getActiveBatchRenderer();
                        if (r) filmstripTex = r->loadTextureFromUri(fsVal.asString());
                    }
                    frames = opts.get("frames", eatscript::Value(64)).asInt();
                    isHorizontal = opts.get("horizontal", eatscript::Value(false)).asBoolean();
                }
            }

            eng.addWidget(static_cast<uint8_t>(WidgetType::Knob),
                          LayoutConstraint::fixed(radius * 2.0f),
                          LayoutConstraint::fixed(radius * 2.0f),
                          accentCol, id, id);

            auto root = eng.getRootNode();
            if (root) {
                std::function<std::shared_ptr<LayoutNode>(const std::shared_ptr<LayoutNode>&)> findLast;
                findLast = [&](const std::shared_ptr<LayoutNode>& n) -> std::shared_ptr<LayoutNode> {
                    if (!n->children.empty()) return findLast(n->children.back());
                    return n;
                };
                auto last = findLast(root);
                if (last && last->id == eng.getScopedId(id)) {
                    last->value = val;
                    last->minValue = minVal;
                    last->maxValue = maxVal;
                    last->textureHandle = filmstripTex;
                    last->totalFrames = frames;
                    last->isHorizontalFilmstrip = isHorizontal;
                    last->styleName = style;
                    last->accentColor = accentCol;
                }
            }
            return eatscript::Value();
        });

    // 7. ui.slider(id, value, opts) or ui.slider({ label: "RESO", value: 0.4, ... })
    evaluator.registerMemberFunction("ui", "slider",
        [explicitEngine, explicitRenderer](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string id = "Slider";
            float val = 0.0f;
            float minVal = 0.0f;
            float maxVal = 1.0f;
            eatscript::Value opts;

            if (args.size() == 1 && args[0].isDict()) {
                opts = args[0];
                id = opts.get("label", opts.get("id", eatscript::Value("Slider"))).asString();
                val = static_cast<float>(opts.get("value", opts.get("default", eatscript::Value(0.0))).asNumber());
                minVal = static_cast<float>(opts.get("min", eatscript::Value(0.0)).asNumber());
                maxVal = static_cast<float>(opts.get("max", eatscript::Value(1.0)).asNumber());
            } else {
                if (args.size() > 0) id = args[0].asString();
                if (args.size() > 1) {
                    if (args[1].isDict()) {
                        opts = args[1];
                        val = static_cast<float>(opts.get("default", opts.get("value", eatscript::Value(0.0))).asNumber());
                        minVal = static_cast<float>(opts.get("min", eatscript::Value(0.0)).asNumber());
                        maxVal = static_cast<float>(opts.get("max", eatscript::Value(1.0)).asNumber());
                    } else {
                        val = static_cast<float>(args[1].asNumber());
                    }
                }
                if (args.size() > 2 && args[2].isDict()) {
                    opts = args[2];
                }
            }

            std::string orient = opts.isDict() ? opts.get("orientation", eatscript::Value("horizontal")).asString() : "horizontal";
            float length = opts.isDict() ? static_cast<float>(opts.get("length", opts.get("height", eatscript::Value(120.0))).asNumber()) : 120.0f;
            float thickness = opts.isDict() ? static_cast<float>(opts.get("thickness", opts.get("width", eatscript::Value(24.0))).asNumber()) : 24.0f;
            std::string accentStr = opts.isDict() ? opts.get("accent", eatscript::Value("#21f4e8")).asString() : "#21f4e8";
            uint32_t accentCol = parseHexColor(accentStr, 0xFF21F4E8);

            uint32_t filmstripTex = 0;
            int frames = 0;
            bool isHorizontal = (orient == "horizontal");
            if (opts.isDict() && (opts.hasKey("filmstrip") || opts.hasKey("texture"))) {
                auto fsVal = opts.hasKey("filmstrip") ? opts.get("filmstrip") : opts.get("texture");
                if (fsVal.isNumber()) {
                    filmstripTex = static_cast<uint32_t>(fsVal.asNumber());
                } else if (fsVal.isString()) {
                    auto* r = explicitRenderer ? explicitRenderer : BatchRenderer2D::getActiveBatchRenderer();
                    if (r) filmstripTex = r->loadTextureFromUri(fsVal.asString());
                }
                frames = opts.get("frames", eatscript::Value(64)).asInt();
            }

            LayoutConstraint main = isHorizontal ? LayoutConstraint::fixed(length) : LayoutConstraint::fixed(thickness);
            LayoutConstraint cross = isHorizontal ? LayoutConstraint::fixed(thickness) : LayoutConstraint::fixed(length);

            eng.addWidget(static_cast<uint8_t>(WidgetType::Slider), main, cross, accentCol, id, id);

            auto root = eng.getRootNode();
            if (root) {
                std::function<std::shared_ptr<LayoutNode>(const std::shared_ptr<LayoutNode>&)> findLast;
                findLast = [&](const std::shared_ptr<LayoutNode>& n) -> std::shared_ptr<LayoutNode> {
                    if (!n->children.empty()) return findLast(n->children.back());
                    return n;
                };
                auto last = findLast(root);
                if (last && last->id == eng.getScopedId(id)) {
                    last->value = val;
                    last->minValue = minVal;
                    last->maxValue = maxVal;
                    last->textureHandle = filmstripTex;
                    last->totalFrames = frames;
                }
            }
            return eatscript::Value();
        });

    // 8. ui.label(text, opts)
    evaluator.registerMemberFunction("ui", "label",
        [explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string text = args.size() > 0 ? args[0].asString() : "";
            eatscript::Value opts = (args.size() > 1 && args[1].isDict()) ? args[1] : eatscript::Value();
            std::string colStr = opts.isDict() ? opts.get("color", eatscript::Value("#ffffff")).asString() : "#ffffff";
            uint32_t color = parseHexColor(colStr, 0xFFFFFFFF);
            float size = opts.isDict() ? static_cast<float>(opts.get("size", eatscript::Value(12.0)).asNumber()) : 12.0f;

            eng.addWidget(static_cast<uint8_t>(WidgetType::Label),
                          LayoutConstraint::flex(0.0f),
                          LayoutConstraint::fixed(size + 4.0f),
                          color, "", text);
            return eatscript::Value();
        });

    // 9. ui.badge(text, opts)
    evaluator.registerMemberFunction("ui", "badge",
        [explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string text = args.size() > 0 ? args[0].asString() : "";
            eatscript::Value opts = (args.size() > 1 && args[1].isDict()) ? args[1] : eatscript::Value();
            std::string colStr = opts.isDict() ? opts.get("color", eatscript::Value("#00ff66")).asString() : "#00ff66";
            uint32_t color = parseHexColor(colStr, 0xFF00FF66);

            eng.addWidget(static_cast<uint8_t>(WidgetType::Badge),
                          LayoutConstraint::fixed(48.0f),
                          LayoutConstraint::fixed(18.0f),
                          color, "", text);
            return eatscript::Value();
        });

    // 10. ui.meter(name, value, opts) or ui.meter({ value: 0.85, ... })
    evaluator.registerMemberFunction("ui", "meter",
        [explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            std::string name = "Meter";
            float val = 0.0f;
            eatscript::Value opts;

            if (args.size() == 1 && args[0].isDict()) {
                opts = args[0];
                name = opts.get("name", opts.get("label", eatscript::Value("Meter"))).asString();
                val = static_cast<float>(opts.get("value", eatscript::Value(0.0)).asNumber());
            } else {
                if (args.size() > 0) name = args[0].asString();
                if (args.size() > 1) val = static_cast<float>(args[1].asNumber());
                if (args.size() > 2 && args[2].isDict()) opts = args[2];
            }

            std::string colStr = opts.isDict() ? opts.get("color", eatscript::Value("#ff3333")).asString() : "#ff3333";
            uint32_t color = parseHexColor(colStr, 0xFFFF3333);

            eng.addWidget(static_cast<uint8_t>(WidgetType::Meter),
                          LayoutConstraint::flex(1.0f),
                          LayoutConstraint::fixed(12.0f),
                          color, name, name);

            auto root = eng.getRootNode();
            if (root) {
                std::function<std::shared_ptr<LayoutNode>(const std::shared_ptr<LayoutNode>&)> findLast;
                findLast = [&](const std::shared_ptr<LayoutNode>& n) -> std::shared_ptr<LayoutNode> {
                    if (!n->children.empty()) return findLast(n->children.back());
                    return n;
                };
                auto last = findLast(root);
                if (last && last->id == eng.getScopedId(name)) {
                    last->value = val;
                }
            }
            return eatscript::Value();
        });

    // 11. ui.spacer(size, isFlex)
    evaluator.registerMemberFunction("ui", "spacer",
        [explicitEngine](const std::vector<eatscript::Value>& args, const auto&) -> eatscript::Value {
            auto& eng = getEngineOrFallback(explicitEngine);
            float sz = args.size() > 0 ? static_cast<float>(args[0].asNumber(8.0)) : 8.0f;
            bool isFlex = args.size() > 1 ? args[1].asBoolean(false) : false;
            eng.addSpacer(sz, isFlex);
            return eatscript::Value();
        });
}

} // namespace eatsbits::ui::clay
