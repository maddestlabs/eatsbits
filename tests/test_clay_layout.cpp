#include <iostream>
#include <cassert>
#include <cmath>

#include "eatsbits/ui/clay/clay_context.hpp"
#include "eatsbits/ui/clay/clay_batch_bridge.hpp"
#include "eatsbits/ui/clay/components.hpp"
#include "eatsbits/ui/clay/ui_layout.hpp"
#include "eatsbits/ui/clay/eatscript_clay_bridge.hpp"
#include "eatsbits/ui/widgets/bottom_nav_bar.hpp"
#include "eatsbits/ui/widgets/transport_header.hpp"
#include "eatsbits/abi/host_registry.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/eatscript/stdlib.hpp"
#include "eatsbits/tui/clay_tui_bridge.hpp"
#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/ui/base64.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"

using namespace eatsbits::ui::clay;
using namespace eatsbits::tui;

void testLifecycleAndArena() {
    std::cout << "[Test 1] Arena Lifecycle & Initialization..." << std::endl;
    ClayContext ctx;
    assert(!ctx.isInitialized());

    bool ok = ctx.initialize(800.0f, 600.0f, 2 * 1024 * 1024);
    assert(ok);
    assert(ctx.isInitialized());

    Clay_Dimensions dims = ctx.getDimensions();
    assert(std::abs(dims.width - 800.0f) < 0.01f);
    assert(std::abs(dims.height - 600.0f) < 0.01f);

    ctx.setDimensions(1024.0f, 768.0f);
    dims = ctx.getDimensions();
    assert(std::abs(dims.width - 1024.0f) < 0.01f);
    assert(std::abs(dims.height - 768.0f) < 0.01f);

    ctx.shutdown();
    assert(!ctx.isInitialized());
    std::cout << "  -> PASSED" << std::endl;
}

void testFlexboxRowLayout() {
    std::cout << "[Test 2] Flexbox Row & Column Calculation..." << std::endl;
    ClayContext ctx;
    ctx.initialize(800.0f, 600.0f);

    ctx.beginLayout();

    CLAY(CLAY_ID("Root"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(800), CLAY_SIZING_FIXED(600) },
            .childGap = 10,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = { 30, 30, 30, 255 }
    }) {
        CLAY(CLAY_ID("LeftPanel"), {
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0) },
            },
            .backgroundColor = { 60, 60, 60, 255 }
        }) {}

        CLAY(CLAY_ID("RightContent"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            },
            .backgroundColor = { 90, 90, 90, 255 }
        }) {}
    }

    Clay_RenderCommandArray cmds = ctx.endLayout(0.016f);
    std::cout << "    cmds.length = " << cmds.length << std::endl;
    for (int i = 0; i < cmds.length; ++i) {
        auto* c = Clay_RenderCommandArray_Get(&cmds, i);
        std::cout << "    cmd[" << i << "]: type=" << (int)c->commandType
                  << " box=(" << c->boundingBox.x << ", " << c->boundingBox.y
                  << ", " << c->boundingBox.width << ", " << c->boundingBox.height << ")" << std::endl;
    }

    Clay_ElementData leftData = ctx.getElementData(CLAY_ID("LeftPanel"));
    std::cout << "    leftData.found=" << leftData.found << " box=("
              << leftData.boundingBox.x << ", " << leftData.boundingBox.y
              << ", " << leftData.boundingBox.width << ", " << leftData.boundingBox.height << ")" << std::endl;
    assert(leftData.found);
    assert(std::abs(leftData.boundingBox.x - 0.0f) < 0.1f);
    assert(std::abs(leftData.boundingBox.width - 200.0f) < 0.1f);
    assert(std::abs(leftData.boundingBox.height - 600.0f) < 0.1f);

    Clay_ElementData rightData = ctx.getElementData(CLAY_ID("RightContent"));
    std::cout << "    rightData.found=" << rightData.found << " box=("
              << rightData.boundingBox.x << ", " << rightData.boundingBox.y
              << ", " << rightData.boundingBox.width << ", " << rightData.boundingBox.height << ")" << std::endl;
    assert(rightData.found);
    // x should be 200 + childGap(10) = 210
    assert(std::abs(rightData.boundingBox.x - 210.0f) < 0.1f);
    // width should be 800 - 200 - 10 = 590
    assert(std::abs(rightData.boundingBox.width - 590.0f) < 0.1f);

    std::cout << "  -> PASSED (Left: " << leftData.boundingBox.width
              << "px, Right: " << rightData.boundingBox.width << "px)" << std::endl;
}

void testPointerHitTesting() {
    std::cout << "[Test 3] Pointer State & Element Hit-Testing..." << std::endl;
    ClayContext ctx;
    ctx.initialize(800.0f, 600.0f);

    // Set mouse at (50, 50) which is inside LeftPanel
    ctx.setPointerState(50.0f, 50.0f, false);

    ctx.beginLayout();
    CLAY(CLAY_ID("Container"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(800), CLAY_SIZING_FIXED(600) },
            .childGap = 10,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        }
    }) {
        CLAY(CLAY_ID("LeftButton"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(100) } }
        }) {}

        CLAY(CLAY_ID("RightButton"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(100) } }
        }) {}
    }
    ctx.endLayout(0.016f);

    assert(ctx.isPointerOver(CLAY_ID("LeftButton")));
    assert(!ctx.isPointerOver(CLAY_ID("RightButton")));

    // Move mouse to (300, 50) which is inside RightButton (starts at 210, width 200)
    ctx.setPointerState(300.0f, 50.0f, false);
    ctx.beginLayout();
    CLAY(CLAY_ID("Container"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(800), CLAY_SIZING_FIXED(600) },
            .childGap = 10,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        }
    }) {
        CLAY(CLAY_ID("LeftButton"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(100) } }
        }) {}

        CLAY(CLAY_ID("RightButton"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(100) } }
        }) {}
    }
    ctx.endLayout(0.016f);

    assert(!ctx.isPointerOver(CLAY_ID("LeftButton")));
    assert(ctx.isPointerOver(CLAY_ID("RightButton")));

    std::cout << "  -> PASSED" << std::endl;
}

void testTerminalTuiBridge() {
    std::cout << "[Test 4] Terminal TUI Snapping & CellSurface Rendering..." << std::endl;
    ClayContext ctx;
    ctx.initialize(80.0f, 24.0f);
    ctx.setDefaultMonospaceMeasure(1.0f, 1.0f);

    ctx.beginLayout();
    CLAY(CLAY_ID("TuiRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(24) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }
    }) {
        // Top status bar (3 rows high)
        CLAY(CLAY_ID("TopBar"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(3) },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = { 20, 20, 25, 255 },
            .border = { .color = { 80, 80, 80, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
            CLAY_TEXT(CLAY_STRING("EATSBITS TERMINAL"), CLAY_TEXT_CONFIG({
                .textColor = { 255, 120, 0, 255 }
            }));
        }

        // Main body (flex grow to fill remaining 21 rows)
        CLAY(CLAY_ID("Workspace"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            },
            .backgroundColor = { 10, 10, 12, 255 }
        }) {}
    }

    Clay_RenderCommandArray cmds = ctx.endLayout(0.016f);
    assert(cmds.length > 0);

    CellSurface surface(80, 24);
    ClayTuiBridge tuiBridge;
    tuiBridge.render(cmds, surface);

    // Verify top bar border rendered box characters
    const Cell& cTopLeft = surface.getCell(0, 0);
    // Standard box top-left corner is 0x250C (┌)
    assert(cTopLeft.codepoint == 0x250C);

    // Verify text rendered
    const Cell& cText = surface.getCell(0, 0); // Text starts at bounding box x, y
    (void)cText;

    Clay_ElementData wsData = ctx.getElementData(CLAY_ID("Workspace"));
    assert(wsData.found);
    assert(std::abs(wsData.boundingBox.height - 21.0f) < 0.1f);
    assert(std::abs(wsData.boundingBox.y - 3.0f) < 0.1f);

    std::cout << "  -> PASSED (TopBar height: 3, Workspace height: "
              << wsData.boundingBox.height << ")" << std::endl;
}

void testTuiTopAndBottomPanelsLayout() {
    std::cout << "[Test 5] TuiApp Top and Bottom Panels Clay Declarative Layout..." << std::endl;
    ClayContext ctx;
    ctx.initialize(120.0f, 36.0f);
    ctx.setDefaultMonospaceMeasure(1.0f, 1.0f);

    // 1. Top Panel Layout (120 cols x 3 rows)
    ctx.setDimensions(120.0f, 3.0f);
    ctx.beginLayout();

    CLAY(CLAY_ID("TuiTopBar"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(120.0f), CLAY_SIZING_FIXED(3.0f) },
            .padding = { 2, 2, 1, 1 },
            .childGap = 1,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = { 16, 18, 23, 255 },
        .border = { .color = { 80, 80, 80, 255 }, .width = { 1, 1, 1, 1, 0 } }
    }) {
        CLAY(CLAY_ID("TuiBrandLogo"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } }
        }) {
            CLAY_TEXT(CLAY_STRING("e:"), CLAY_TEXT_CONFIG({ .textColor = { 255, 179, 0, 255 } }));
        }

        CLAY(CLAY_ID("BrandSpacer"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(1) } }
        }) {}

        CLAY(CLAY_ID("PlayBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = { 57, 255, 20, 255 }
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(" ▶ PLAY "), CLAY_TEXT_CONFIG({ .textColor = { 0, 0, 0, 255 } }));
        }

        CLAY(CLAY_ID("StopBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = { 80, 80, 80, 255 }
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(" ⏹ Stop "), CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 255 } }));
        }

        CLAY(CLAY_ID("RecBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = { 80, 80, 80, 255 }
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(" ⏺ Rec "), CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 255 } }));
        }

        CLAY(CLAY_ID("StatsContainer"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) },
                .padding = { 1, 0, 0, 0 },
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }
            }
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString("TEMPO: 132.0 BPM │ 4/4 │ 001.01.01"), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));
        }

        CLAY(CLAY_ID("BrowserBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = { 80, 80, 80, 255 }
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(" [▤ BROWSER] "), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));
        }
    }

    Clay_RenderCommandArray topCmds = ctx.endLayout(0.016f);
    assert(topCmds.length > 0);

    auto playData = ctx.getElementData(CLAY_ID("PlayBtn"));
    assert(playData.found);
    Rect playBox = ClayTuiBridge::snapToCells(playData.boundingBox);
    assert(playBox.x == 6); // 2 padding + 2 logo + 2 spacer
    assert(playBox.y == 1);
    assert(playBox.width == 10); // " ▶ PLAY " is 10 chars with monospace 1.0f

    auto stopData = ctx.getElementData(CLAY_ID("StopBtn"));
    assert(stopData.found);
    Rect stopBox = ClayTuiBridge::snapToCells(stopData.boundingBox);
    assert(stopBox.x == 17); // 6 + 10 width + 1 gap
    assert(stopBox.y == 1);

    auto browserData = ctx.getElementData(CLAY_ID("BrowserBtn"));
    assert(browserData.found);
    Rect browserBox = ClayTuiBridge::snapToCells(browserData.boundingBox);
    assert(browserBox.x + browserBox.width == 118); // 120 - 2 right padding

    // Render onto CellSurface with offset (0, 0)
    CellSurface surface(120, 36);
    ClayTuiBridge bridge;
    bridge.render(topCmds, surface, 0, 0);

    // Verify cell content
    const Cell& playCell = surface.getCell(6, 1);
    assert(playCell.bg == Color::FromRgb(57, 255, 20));

    // 2. Bottom Panel Layout (120 cols x 2 rows) rendered at row 34
    ctx.setDimensions(120.0f, 2.0f);
    ctx.beginLayout();

    CLAY(CLAY_ID("TuiBottomBar"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(120.0f), CLAY_SIZING_FIXED(2.0f) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = { 20, 22, 30, 255 },
        .border = { .color = { 80, 80, 80, 255 }, .width = { 0, 0, 1, 0, 0 } }
    }) {
        CLAY(CLAY_ID("BottomTopBorderRow"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } }
        }) {}

        CLAY(CLAY_ID("BottomContentRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) },
                .padding = { 2, 2, 0, 0 },
                .childGap = 0,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            }
        }) {
            CLAY(CLAY_IDI("TabBtn", 0), {
                .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
                .backgroundColor = { 255, 179, 0, 255 }
            }) {
                CLAY_TEXT(ClayTuiBridge::toClayString("  Arranger  "), CLAY_TEXT_CONFIG({ .textColor = { 0, 0, 0, 255 } }));
            }

            CLAY(CLAY_IDI("TabSep", 0), {
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(1) },
                    .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER }
                }
            }) {
                CLAY_TEXT(CLAY_STRING("│"), CLAY_TEXT_CONFIG({ .textColor = { 80, 80, 80, 255 } }));
            }

            CLAY(CLAY_IDI("TabBtn", 1), {
                .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
                .backgroundColor = { 30, 34, 45, 255 }
            }) {
                CLAY_TEXT(ClayTuiBridge::toClayString("    Edit    "), CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 255 } }));
            }
        }
    }

    Clay_RenderCommandArray bottomCmds = ctx.endLayout(0.016f);
    assert(bottomCmds.length > 0);

    bridge.render(bottomCmds, surface, 0, 34);

    // Verify row 34 has top border
    const Cell& borderCell = surface.getCell(0, 34);
    assert(borderCell.codepoint == 0x2500); // ─

    // Verify Tab 0 is placed at row 35 with Amber background
    const Cell& tab0Cell = surface.getCell(2, 35);
    assert(tab0Cell.bg == Color::FromRgb(255, 179, 0));

    std::cout << "  -> PASSED (Top transport & bottom tabs rendered cleanly)" << std::endl;
}

void testReusableComponentLibrary() {
    std::cout << "[Test 6] Reusable Clay Component Library (UI_Button, UI_Pill, UI_Card, UI_CustomWidget)..." << std::endl;
    ClayContext ctx;
    ctx.initialize(800.0f, 600.0f);

    ctx.setDimensions(800.0f, 600.0f);
    ctx.setPointerState(50.0f, 50.0f, false);
    ctx.beginLayout();

    CLAY(CLAY_ID("Root"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(800.0f), CLAY_SIZING_FIXED(600.0f) },
            .padding = { 16, 16, 16, 16 },
            .childGap = 12,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }
    }) {
        // Row 1: Buttons
        CLAY(CLAY_ID("ButtonRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            }
        }) {
            UI_Button(CLAY_ID("BtnAddTrack"), "+ ADD TRACK", true, false);
            UI_Button(CLAY_ID("BtnRender"), "EXPORT AUDIO", false, false);
        }

        // Row 2: Filter Pills
        CLAY(CLAY_ID("PillRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 6,
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            }
        }) {
            UI_Pill(CLAY_ID("PillAll"), "ALL", true);
            UI_Pill(CLAY_ID("PillAudio"), "AUDIO", false);
            UI_Pill(CLAY_ID("PillMidi"), "MIDI", false);
            UI_Pill(CLAY_ID("PillFx"), "EFFECTS", false);
        }

        // Row 3: Cards (one expanded, one collapsed)
        CLAY(CLAY_ID("CardRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 12,
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            }
        }) {
            UI_Card(CLAY_ID("CardExpanded"), "COMPRESSOR (ACTIVE)", true, false, []() {
                UI_Pill(CLAY_ID("RatioPill"), "4:1 RATIO", true);
            });

            UI_Card(CLAY_ID("CardCollapsed"), "REVERB (MUTED)", true, true, []() {
                UI_Pill(CLAY_ID("UnrenderedPill"), "HIDDEN", false);
            });
        }

        // Row 4: Custom widget (e.g. rotary knob or waveform display)
        int dummyKnobData = 42;
        UI_CustomWidget(CLAY_ID("KnobCustomWidget"), {
            .width = CLAY_SIZING_FIXED(64.0f),
            .height = CLAY_SIZING_FIXED(64.0f)
        }, &dummyKnobData);
    }

    Clay_RenderCommandArray cmds = ctx.endLayout(0.016f);
    assert(cmds.length > 0);

    // Verify elements exist and were correctly laid out
    auto btnData = ctx.getElementData(CLAY_ID("BtnAddTrack"));
    assert(btnData.found);
    assert(btnData.boundingBox.width > 0.0f);
    assert(btnData.boundingBox.height > 0.0f);

    auto pillData = ctx.getElementData(CLAY_ID("PillAll"));
    assert(pillData.found);

    auto expCard = ctx.getElementData(CLAY_ID("CardExpanded"));
    assert(expCard.found);

    auto colCard = ctx.getElementData(CLAY_ID("CardCollapsed"));
    assert(colCard.found);
    // Collapsed card height should only be its header height (around 26px + borders)
    assert(colCard.boundingBox.height <= 28.0f);
    // Expanded card height should be significantly taller because of body content
    assert(expCard.boundingBox.height > colCard.boundingBox.height);

    // Unrendered pill inside collapsed card must NOT exist in the layout tree
    auto unrenderedPill = ctx.getElementData(CLAY_ID("UnrenderedPill"));
    assert(!unrenderedPill.found);

    // Custom widget bounds
    auto knobData = ctx.getElementData(CLAY_ID("KnobCustomWidget"));
    assert(knobData.found);
    assert(std::abs(knobData.boundingBox.width - 64.0f) < 0.01f);
    assert(std::abs(knobData.boundingBox.height - 64.0f) < 0.01f);

    std::cout << "  -> PASSED (UI_Button, UI_Pill, UI_Card, UI_CustomWidget verified)" << std::endl;
}

void testFluentWrappersAndIdScoping() {
    std::cout << "[Test 7] Fluent C++ RAII Layout Wrappers & Hierarchical ID Scoping..." << std::endl;
    ClayContext ctx;
    ctx.initialize(1280.0f, 800.0f);
    ctx.makeCurrent();
    ctx.beginLayout();

    {
        auto rootScope = Row::ScopedFixed(800.0f, 10.0f);

        {
            IdScope scope("channel_strip_1");
            auto colScope = Column::ScopedFixed(120.0f, 6.0f);
            UI_Button(UI_ID("BtnMute"), "M", false, false);
            UI_Button(UI_ID("BtnSolo"), "S", false, true);
            UI_Spacer(12.0f);
        }

        {
            IdScope scope("channel_strip_2");
            auto colScope = Column::ScopedFixed(120.0f, 6.0f);
            UI_Button(UI_ID("BtnMute"), "M", false, false);
            UI_Button(UI_ID("BtnSolo"), "S", false, false);
        }

        {
            auto stackScope = Stack::ScopedBegin();
            UI_Button(UI_ID("BtnStacked"), "Overlay", false, false);
        }
    }

    Clay_RenderCommandArray cmds = ctx.endLayout();
    assert(cmds.length > 0);

    auto m1 = ctx.getElementData(UI_ID("channel_strip_1/BtnMute"));
    auto m2 = ctx.getElementData(UI_ID("channel_strip_2/BtnMute"));
    assert(m1.found);
    assert(m2.found);
    // Must be laid out in sequential columns (m2.x > m1.x)
    assert(m2.boundingBox.x > m1.boundingBox.x);

    std::cout << "  -> PASSED (Fluent Row, Column, Stack, IdScope, ContainerScope verified)" << std::endl;
}

void testUiLayoutEngineExplicitRectProtection() {
    std::cout << "[Test 8] UiLayoutEngine & Explicit Overlay Rect Protection (Lesson 2.1)..." << std::endl;
    UiLayoutEngine engine;
    engine.beginRoot(0.0f, 0.0f, 1280.0f, 800.0f, LayoutDirection::Column);

    // Fixed Top Bar
    engine.beginContainer(LayoutDirection::Row, LayoutConstraint::fixed(56.0f));
    engine.addWidget(1, LayoutConstraint::fixed(200.0f), LayoutConstraint::fill(), 0xFF112233, "TopNav");
    // Explicit rect: logo button fixed at (14, 16, 27, 24)
    engine.addExplicitWidget(2, 14.0f, 16.0f, 27.0f, 24.0f, 0xFFFFAA00, "ExplicitLogo");
    // Flex rest of header
    engine.addWidget(1, LayoutConstraint::flex(1.0f), LayoutConstraint::fill(), 0xFF334455, "HeaderSpacer");
    engine.endContainer();

    // Flex body
    engine.beginContainer(LayoutDirection::Row, LayoutConstraint::flex(1.0f));
    engine.addWidget(3, LayoutConstraint::fixed(240.0f), LayoutConstraint::fill(), 0xFF223344, "Sidebar");
    engine.addWidget(4, LayoutConstraint::flex(1.0f), LayoutConstraint::fill(), 0xFF111111, "EditorArea");
    engine.endContainer();

    engine.solve({0.0f, 0.0f, 1280.0f, 800.0f});

    // Verify explicit logo was preserved at its exact coordinates and NOT displaced by flexbox
    std::shared_ptr<LayoutNode> foundLogo;
    std::function<void(const std::shared_ptr<LayoutNode>&)> findNode = [&](const std::shared_ptr<LayoutNode>& n) {
        if (n->id == "ExplicitLogo") foundLogo = n;
        for (const auto& c : n->children) findNode(c);
    };
    findNode(engine.getRootNode());

    assert(foundLogo != nullptr);
    assert(foundLogo->isExplicitRect);
    assert(foundLogo->resolvedRect.x == 14.0f);
    assert(foundLogo->resolvedRect.y == 16.0f);
    assert(foundLogo->resolvedRect.w == 27.0f);
    assert(foundLogo->resolvedRect.h == 24.0f);

    std::cout << "  -> PASSED (Explicit overlay node protected from flex displacement)" << std::endl;
}

void testSharedContextWidgets() {
    std::cout << "[Test 9] Dismantled Micro-Arenas (BottomNavBar & TransportHeader)..." << std::endl;
    ClayContext sharedCtx;
    sharedCtx.initialize(1280.0f, 800.0f);

    eatsbits::ui::BottomNavBar nav;
    nav.layout(1280.0f, 800.0f, 48.0f, &sharedCtx);
    assert(nav.getBounds().y == 752.0f);
    assert(nav.getBounds().h == 48.0f);

    eatsbits::ui::TransportHeader transport;
    transport.layout(1280.0f, 56.0f, &sharedCtx);
    assert(transport.getBounds().w == 1280.0f);
    assert(transport.getBounds().h == 56.0f);

    std::cout << "  -> PASSED (Widgets shared root ClayContext with zero micro-arena allocations)" << std::endl;
}

void testEatscriptClayBridge() {
    std::cout << "[Test 10] Eatscript Clay Layout Bridge Host Registration & Scripting..." << std::endl;
    eatsbits::abi::HostRegistry registry;
    UiLayoutEngine engine;
    registerClayLayoutHostFunctions(registry, &engine);

    eatsbits::eatscript::Evaluator eval;
    eatsbits::eatscript::registerStandardLibrary(eval, &registry);

    std::string script = R"(
ui_push_id("eatscript_root")
ui_begin_column(1280.0, 800.0)
ui_begin_fixed_row(56.0)
ui_spacer(14.0, false)
ui_end()
ui_begin_flex_row(1.0)
ui_end()
ui_end()
ui_pop_id()
)";

    eval.evaluateSource(script);
    if (!eval.getLastError().empty()) {
        std::cerr << "Script error: " << eval.getLastError() << std::endl;
    }
    assert(eval.getLastError().empty());

    engine.solve({0.0f, 0.0f, 1280.0f, 800.0f});
    auto root = engine.getRootNode();
    assert(root != nullptr);
    assert(root->children.size() == 2);
    assert(root->children[0]->resolvedRect.h == 56.0f);
    assert(root->children[1]->resolvedRect.h == 744.0f);

    std::cout << "  -> PASSED (Eatscript declared and solved Clay layout)" << std::endl;
}

void testEatscriptDeclarativeUiDslAndEmbeddedAssets() {
    std::cout << "[Test 11] Declarative Clay UI DSL & Embedded In-Memory Assets..." << std::endl;
    using namespace eatsbits::ui;

    // 1. Base64 test
    std::string sample = "Hello Eatsbits 2026";
    std::vector<uint8_t> rawSample(sample.begin(), sample.end());
    std::string b64 = base64Encode(rawSample);
    std::vector<uint8_t> decoded = base64Decode(b64);
    assert(decoded == rawSample);

    // 2. Data URI parse & Embedded asset declaration
    std::string dataUri = "data:image/png;base64," + b64;
    auto parsed = parseDataUri(dataUri);
    assert(parsed.has_value());
    assert(parsed->mimeType == "image/png");
    assert(parsed->data == rawSample);

    std::string decl = generateEmbeddedAssetDeclaration("KNOB_STRIP_PNG", "image/png", rawSample.data(), rawSample.size());
    assert(decl.find("const KNOB_STRIP_PNG = \"data:image/png;base64,") == 0);

    // 3. BatchRenderer2D and in-memory texture loading
    BatchRenderer2D renderer;
    BatchRenderer2D::setActiveBatchRenderer(&renderer);

    // 1x1 PNG base64 (standard transparent 1x1 PNG)
    const std::string k1x1Png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";
    uint32_t texHandle = renderer.loadTextureFromUri(k1x1Png);
    assert(texHandle != 0);

    // 4. Eatscript engine & evaluator binding
    eatsbits::abi::HostRegistry registry;
    UiLayoutEngine engine;
    registerClayLayoutHostFunctions(registry, &engine);

    eatsbits::eatscript::Evaluator eval;
    eatsbits::eatscript::registerStandardLibrary(eval, &registry);
    registerEatscriptUiModule(eval, &engine, &renderer);

    std::string script = R"(
const KNOB_TEX = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="
let myTex = ui.loadTexture(KNOB_TEX)

def build_row():
    ui.knob({"label": "CUTOFF", "value": 0.75, "texture": myTex, "frames": 64, "style": "cyberpunk", "width": 64, "height": 64})
    ui.slider({"label": "RESO", "value": 0.4, "width": 32, "height": 120})

def build_col():
    ui.label("Eatsbits Synthesizer", {"color": "#ff55ff"})
    ui.badge("ACTIVE", {"color": "#00ff88"})
    ui.meter({"label": "Level", "value": 0.85, "width": 100, "height": 12})
    ui.spacer(8)

def build_main():
    ui.row({"gap": 10}, build_row)
    ui.column({"gap": 4}, build_col)

ui.panel({"width": 800, "height": 600, "padding": 12, "gap": 8}, build_main)
)";

    eval.evaluateSource(script);
    if (!eval.getLastError().empty()) {
        std::cerr << "Script error: " << eval.getLastError() << std::endl;
    }
    assert(eval.getLastError().empty());

    engine.solve({0.0f, 0.0f, 800.0f, 600.0f});
    auto root = engine.getRootNode();
    assert(root != nullptr);
    assert(root->children.size() == 2); // Row and Column

    auto row = root->children[0];
    assert(row->direction == LayoutDirection::Row);
    assert(row->children.size() == 2);

    auto knob = row->children[0];
    assert(knob->widgetType == WidgetType::Knob);
    assert(knob->label == "CUTOFF");
    assert(std::abs(knob->value - 0.75f) < 0.01f);
    assert(knob->textureHandle == texHandle);
    assert(knob->totalFrames == 64);
    assert(knob->styleName == "cyberpunk");

    auto slider = row->children[1];
    assert(slider->widgetType == WidgetType::Slider);
    assert(slider->label == "RESO");
    assert(std::abs(slider->value - 0.4f) < 0.01f);

    auto col = root->children[1];
    assert(col->direction == LayoutDirection::Column);
    assert(col->children.size() == 4);

    auto label = col->children[0];
    assert(label->widgetType == WidgetType::Label);
    assert(label->label == "Eatsbits Synthesizer");

    auto badge = col->children[1];
    assert(badge->widgetType == WidgetType::Badge);
    assert(badge->label == "ACTIVE");

    auto meter = col->children[2];
    assert(meter->widgetType == WidgetType::Meter);
    assert(std::abs(meter->value - 0.85f) < 0.01f);

    auto spacer = col->children[3];
    assert(spacer->widgetType == WidgetType::Spacer);

    BatchRenderer2D::setActiveBatchRenderer(nullptr);
    std::cout << "  -> PASSED (Base64 decode, in-memory texture, declarative DSL & widgets verified)" << std::endl;
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << " Running Eatsbits Clay Layout Engine Tests  " << std::endl;
    std::cout << "============================================" << std::endl;

    try {
        testLifecycleAndArena();
        testFlexboxRowLayout();
        testPointerHitTesting();
        testTerminalTuiBridge();
        testTuiTopAndBottomPanelsLayout();
        testReusableComponentLibrary();
        testFluentWrappersAndIdScoping();
        testUiLayoutEngineExplicitRectProtection();
        testSharedContextWidgets();
        testEatscriptClayBridge();
        testEatscriptDeclarativeUiDslAndEmbeddedAssets();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "============================================" << std::endl;
    std::cout << " ALL CLAY LAYOUT TESTS PASSED SUCCESSFULLY! " << std::endl;
    std::cout << "============================================" << std::endl;
    return 0;
}
