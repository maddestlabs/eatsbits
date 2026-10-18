#pragma once

#include "eatsbits/abi/host_registry.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/ui/clay/ui_layout.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"

namespace eatsbits::ui::clay {

/**
 * @brief Sets the active UiLayoutEngine for scripted layout evaluation.
 */
void setActiveUiLayoutEngine(UiLayoutEngine* engine);

/**
 * @brief Gets the active UiLayoutEngine.
 */
[[nodiscard]] UiLayoutEngine* getActiveUiLayoutEngine();

/**
 * @brief Registers declarative UI layout host functions with the Eatscript HostRegistry.
 * Exposes:
 *  - ui_begin_column(w, h) or (h) or ()
 *  - ui_begin_row(w, h) or (w) or ()
 *  - ui_begin_fixed_row(height)
 *  - ui_begin_flex_row(weight)
 *  - ui_begin_fixed_column(width)
 *  - ui_begin_flex_column(weight)
 *  - ui_begin_stack()
 *  - ui_end() / ui_end_container()
 *  - ui_spacer(size, is_flex)
 *  - ui_push_id(id)
 *  - ui_pop_id()
 *  - ui_get_id(raw_id)
 *  - ui_load_texture(uri)
 */
void registerClayLayoutHostFunctions(abi::HostRegistry& registry,
                                     UiLayoutEngine* explicitEngine = nullptr,
                                     BatchRenderer2D* explicitRenderer = nullptr);

/**
 * @brief Registers the high-level declarative Eatscript UI DSL module into the evaluator.
 * Exposes the `ui` object namespace with:
 *  - ui.panel(opts, callback)
 *  - ui.row(opts, callback)
 *  - ui.column(opts, callback)
 *  - ui.stack(opts, callback)
 *  - ui.knob(id, value, opts)
 *  - ui.slider(id, value, opts)
 *  - ui.label(text, opts)
 *  - ui.badge(text, opts)
 *  - ui.meter(name, value, opts)
 *  - ui.spacer(size, isFlex)
 *  - ui.loadTexture(uri)
 */
void registerEatscriptUiModule(eatscript::Evaluator& evaluator,
                               UiLayoutEngine* explicitEngine = nullptr,
                               BatchRenderer2D* explicitRenderer = nullptr);

} // namespace eatsbits::ui::clay
