# Lightsword Architecture Blueprint: Transitioning to the Eatsbits Core
**Document:** `lightsword-eatscript.md`  
**Target:** Lightsword Bible Study Engine & Worldview Consistency System  
**Underlying Platform:** Eatsbits Core (C++20, HarfBuzz, Clay Layout, WebGPU/OpenGL, Eatscript Runtime)

---

## 1. Executive Summary & Vision

**Lightsword** is transitioning from a traditional Flutter-based Bible reading and study app into a high-performance, programmable theological research engine built on the **Eatsbits Core**.

The primary ambition of this transition is twofold:
1. **Unrivaled Textual & Visual Precision:** Direct, native control over Biblical Hebrew (niqqud, cantillation, RTL) and polytonic Greek text shaping via HarfBuzz, combined with an ultra-fast immediate-mode spatial canvas (Clay Layout + 2D GPU batching) for interactive **Discourse Analysis (Arcing)**.
2. **A Formal Biblical Worldview Consistency Engine:** Leveraging **Eatscript** (the embedded scripting language of Eatsbits) to compile arced discourse graphs into formal propositional logic and deductive rules (Datalog/SMT), enabling automated cross-passage consistency checking, semantic nuance testing, and systematic worldview modeling.

---

## 2. Framework Comparison: Flutter vs. Eatsbits Core

| Dimension | Flutter Implementation | Eatsbits Core (`eatsbits` + `eatscript`) |
| :--- | :--- | :--- |
| **Language & Runtime** | Dart (AOT/JIT), Flutter Engine | C++20 + Eatscript (Lightweight embedded VM) |
| **Complex Script Shaping** | Skia / Impeller text painters (abstracted, high overhead for glyph-level coordinates) | **Direct HarfBuzz + FreeType integration**: Direct access to glyph clusters, advances, and bounding boxes for sub-word arc snapping |
| **Arcing & Spatial UI** | `CustomPainter` with Flutter RenderObjects (heavy widget tree rebuilds on zoom/pan) | **Clay Micro-Layout + GPU BatchRenderer**: Zero-allocation declarative layout; Bezier curves rendered via batched vertex buffers |
| **Scriptability & DSL** | Static compiled Dart; user scripts require eval/third-party interpreters | **First-class Eatscript DSL**: Real-time evaluation of discourse arcs, hermeneutical hypotheses, and dynamic logic queries |
| **Logic & Inference** | Heavy external Dart packages or remote API calls | **Embedded C++ Logic Subsystem**: In-process Datalog / miniKanren / SMT solver accessible directly from Eatscript |
| **Binary Footprint & Performance** | 40MB+ Flutter runtime overhead, garbage-collected heap | Minimal binary size, sub-millisecond startup, deterministic memory footprint |

---

## 3. Core Architectural Subsystems

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                             LIGHTSWORD GUI                                  │
│   Clay Declarative UI + HarfBuzz Multi-Script Text Canvas + Vector Arcs     │
│   (Scripture View | Interlinear Inspector | Discourse Canvas | Logic Hub)   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                            EATSCRIPT RUNTIME                                │
│   - Arcing & Propositional DSL (`discourse.*`)                              │
│   - Biblical Content & Strong's Query API (`bible.*`)                        │
│   - Semantic Nuance & Worldview Logic Engine (`logic.*`)                    │
└───────────────────┬──────────────────────────────────┬──────────────────────┘
                    │                                  │
┌───────────────────▼──────────────────┐ ┌─────────────▼──────────────────────┐
│          STEP BIBLE HOST API         │ │   WORLDVIEW CONSISTENCY SOLVER     │
│  - Embedded SQLite (WLC, NA28, ESV)  │ │   - Datalog / miniKanren Engine    │
│  - Morphology & Semantic Domains     │ │   - Arc DAG to Horn Clause Mapper  │
│  - Strong's Concordance Graph        │ │   - Contradiction & Tension Tracer │
└──────────────────────────────────────┘ └────────────────────────────────────┘
```

---

## 4. Subsystem Specifications

### 4.1 Biblical Typography & Text Shaping (HarfBuzz)
Biblical languages impose rigorous typographic demands:
* **Biblical Hebrew & Aramaic:** Right-to-Left (RTL), consonants with multi-level diacritics (niqqud vowel points, dagesh/mappiq, and cantillation accents/te'amim). Misaligned accents alter grammatical meaning.
* **Biblical Greek:** Polytonic accents (acute, grave, circumflex, rough/smooth breathings, iota subscripts).

**How Eatsbits Solves This:**
* The C++ engine passes UTF-8 text directly to `hb_buffer_t` with explicit script tags (`HB_SCRIPT_HEBREW`, `HB_SCRIPT_GREEK`), direction (`HB_DIRECTION_RTL`, `HB_DIRECTION_LTR`), and language (`hb_language_from_string("he", -1)`).
* `hb_shape()` produces exact glyph positions and cluster indices.
* **Glyph Cluster Mapping:** Every glyph run retains a byte-offset back to the source text token and Strong's number. When an arc connector attaches to a specific word or phrase, the anchor coordinate $(x, y)$ is calculated directly from HarfBuzz glyph advances, ensuring pixel-perfect arc attachment under all zoom levels.

### 4.2 STEP Bible Data Integration & Host API
STEP Bible provides high-quality open data:
* Hebrew Old Testament (Westminster Leningrad Codex with morphology and Strong's).
* Greek New Testament (Tyndale House / NA28 / TR with morphology and Strong's).
* Ancient translations, lexicons (BDB, Thayer, Abbott-Smith), and semantic domains.

**Storage & Retrieval:**
* Packaged into an optimized, embedded SQLite database (`step_bible.db`).
* The C++ core provides memory-mapped indexing for instantaneous cross-references and lemma lookups.
* Exposed to Eatscript via the `bible` host module:
  ```javascript
  const tokens = bible.getTokens("Rom.3.28");
  // [
  //   { surface: "δικαιούσθαι", lemma: "δικαιόω", strong: "G1344", morph: "V-PNP", offset: 12 },
  //   { surface: "πίστει", lemma: "πίστις", strong: "G4102", morph: "N-DSF", offset: 25 },
  //   ...
  // ]
  ```

---

## 5. Discourse Analysis (Arcing) & Visual Canvas

### 5.1 The Arcing Methodology
Bible arcing decomposes a passage into **propositions** (units of thought) and connects them with **relational arcs** indicating logical flow:

* **Support Relations:** Ground (G), Cause (C), Means (M).
* **Logical Relations:** Inference (I), Result (R), Purpose (P), Condition (Cd).
* **Adversative Relations:** Concession (Cs), Contrast (Ct), Alternative (A).
* **Coordinate Relations:** Series (S), Progression (Pr).

### 5.2 Clay + GPU Vector Arc Rendering
In the UI:
1. Proposition text blocks are laid out using **Clay** containers (`CLAY_LAYOUT`).
2. Each proposition node exposes connection points (anchors at top/bottom/sides).
3. The Eatsbits `BatchRenderer2D` renders dynamic cubic Bezier curves connecting the nodes:
   $$\mathbf{B}(t) = (1-t)^3 \mathbf{P}_0 + 3(1-t)^2 t \mathbf{P}_1 + 3(1-t)t^2 \mathbf{P}_2 + t^3 \mathbf{P}_3$$
4. Arcs are color-coded by category (e.g., Logical = Cyan, Adversative = Amber, Support = Green).
5. Hovering an arc highlights the entire logical subtree; clicking allows editing the relation type or inspecting the underlying logical deduction.

---

## 6. Eatscript: The Semantic & Logic System

Eatscript acts as the executable bridge between discourse diagrams and formal worldview propositions.

### 6.1 Declarative Arcing in Eatscript
```javascript
import { ui, bible, discourse, logic } from "eatsbits";

export default discourse.defineArc({
    id: "romans_3_28",
    reference: "Romans 3:28",
    propositions: [
        {
            id: "P1",
            text: "For we hold that one is justified by faith",
            tokens: ["G3049", "G444", "G1344", "G4102"],
            head: true
        },
        {
            id: "P2",
            text: "apart from works of the law.",
            tokens: ["G5565", "G2041", "G3551"],
            head: false
        }
    ],
    relations: [
        {
            type: "manner_negative",
            parent: "P1",
            child: "P2",
            description: "Exclusion of law-works as grounds for justification"
        }
    ],
    
    // Formal translation into propositional claims
    formalize(ctx) {
        return [
            logic.claim("Justified(person, ByFaith)"),
            logic.claim("¬Justified(person, ByWorksOfLaw)")
        ];
    }
});
```

### 6.2 The Worldview Consistency Engine (Cross-Passage Validation)

The consistency engine evaluates propositions extracted from different arcs across the canon to identify **theological tensions, apparent contradictions, and harmonizing semantic nuances**.

#### Example: Reconciling Romans 3:28 and James 2:24

```javascript
import { logic, discourse } from "eatsbits";
import rom3 from "./arcs/romans_3_28.eats";
import jas2 from "./arcs/james_2_24.eats";

// Define Semantic Domains from STEP Bible Lexical Graphs
logic.defineConcept("Pistis", {
    MereIntellectualAssent: { strong: "G4102", context: "James 2:19 ('demons believe')" },
    FiduciaryAllegiance:   { strong: "G4102", context: "Romans 4:3 ('Abraham trusted God')" }
});

logic.defineConcept("Dikaioo", {
    ForensicAcquittal:     { strong: "G1344", context: "Romans 3:24 ('declared righteous')" },
    PublicVindication:     { strong: "G1344", context: "James 2:21 ('offering Isaac')" }
});

// Run Consistency Evaluation
const model = logic.createModel("SolusFideVsJames");
model.addArc(rom3);
model.addArc(jas2);

// Evaluation with Default (Flat) Semantics
const naiveResult = model.checkConsistency();
// naiveResult.consistent == false
// naiveResult.tension == "Direct opposition on Justified(person, FaithOnly)"

// Apply Semantic Nuance Resolution (Hermeneutical Hypothesis)
model.hypothesize({
    mapping: [
        { passage: "Rom.3.28", concept: "Dikaioo", resolveTo: "ForensicAcquittal" },
        { passage: "Rom.3.28", concept: "Pistis",  resolveTo: "FiduciaryAllegiance" },
        { passage: "Jas.2.24", concept: "Dikaioo", resolveTo: "PublicVindication" },
        { passage: "Jas.2.24", concept: "Pistis",  resolveTo: "MereIntellectualAssent" }
    ]
});

const resolvedResult = model.checkConsistency();
// resolvedResult.consistent == true!
// Generates explanation trace showing how both statements are simultaneously true.
```

---

## 7. Logic Engine Options: Datalog vs. SMT vs. miniKanren

| Engine Type | Strengths for Biblical Studies | Implementation Strategy in Eatsbits |
| :--- | :--- | :--- |
| **Datalog (Deductive DB)** *(Recommended)* | Exceptional for recursive relationships, genealogies, covenantal structures, and Horn-clause deductions ($A \land B \implies C$). Guaranteed polynomial termination. | Embed a lightweight C++ Datalog evaluator (or SQLite Recursive CTEs / Soufflé C++ API) exposed via `logic.query()`. |
| **miniKanren (Relational Logic)** | Bidirectional inference: can ask *"What premises must be true for this theological conclusion to hold?"* | Embed a minimal C++ microKanren runtime (~300 LOC) with Eatscript bindings. |
| **SMT Solver (e.g., Z3 / CVC5)** | Full first-order logic with quantifiers, arithmetic, and uninterpreted functions. | Link via C API for advanced contradiction refutation proofs. |

---

## 8. Gap Analysis & Transition Checklist for Existing Lightsword Codebase

When evaluating the existing Flutter codebase against this blueprint, look for and map the following modules:

### 1. Database & Asset Layer
- [ ] **STEP Bible SQLite Extraction:** Locate where `.sqlite` or JSON raw data files reside in Lightsword (lexicons, morphology, interlinear tables) and plan their migration to `assets/bible/step_bible.db`.
- [ ] **Morphology Parser:** Port the Greek/Hebrew morphological code parser (e.g., parsing `V-AAP-NMS` to Part-of-Speech/Tense/Voice/Mood) into C++ helper functions or Eatscript modules.

### 2. Typography & Rendering
- [ ] **Font Assets:** Ensure high-quality fonts with full Biblical Hebrew & Greek OpenType glyph tables (e.g., *SBL Hebrew*, *SBL Greek*, *Cardo*, or *Ezra SIL*) are packaged as embedded assets.
- [ ] **BiDi & HarfBuzz Buffer:** Verify text shaping pipeline handles mixed RTL (Hebrew) and LTR (Strong's tags / English glosses) within the same line view.

### 3. Arcing State & File Format
- [ ] **Discourse Schema Definition:** Compare Lightsword's current schema for storing user notes or arcing data with the proposed `.eats` / JSON discourse model.
- [ ] **Interactive Gestures:** Map pan, pinch-zoom, and connector dragging from Flutter `GestureDetector` to Clay mouse/touch events and bounding box queries.

### 4. Logic & Proposition Storage
- [ ] **Proposition Extractor:** How are proposition boundaries currently marked by users? (Sentence splitting rules, clause-level delimiter tokens).
- [ ] **Consistency Rules Repository:** Establish a structured directory (e.g., `theology/axioms/`, `theology/models/`) where users can store canonical propositions and test hypotheses.

---

## 9. Conclusion & Next Steps

Transitioning Lightsword to the Eatsbits core turns the app from a passive reader into an **interactive ideological compiler and worldview laboratory**. 

* **Next Step for Lightsword Repo:** Compare your existing Flutter models, SQLite schema, and UI layouts against the four subsystems detailed in Section 4.
* **Next Step for Eatsbits Core:** Verify HarfBuzz Hebrew/Greek shaping in a sample Clay window, then implement the initial `discourse` and `logic` bridge in Eatscript.
