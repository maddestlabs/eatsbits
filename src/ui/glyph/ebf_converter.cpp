#include "eatsbits/ui/glyph/ebf_converter.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

namespace eatsbits::ui::glyph {

namespace {

struct DecomposeContext {
    std::vector<uint8_t>* commandStream{nullptr};
    float scale{1.0f};

    static void pushInt16(std::vector<uint8_t>& out, int16_t val) {
        out.push_back(static_cast<uint8_t>(val & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    }
};

static int ebfMoveTo(const FT_Vector* to, void* user) {
    auto* ctx = static_cast<DecomposeContext*>(user);
    int16_t x = static_cast<int16_t>(std::round(static_cast<float>(to->x) * ctx->scale));
    int16_t y = static_cast<int16_t>(std::round(static_cast<float>(to->y) * ctx->scale));

    ctx->commandStream->push_back(static_cast<uint8_t>(EbfOpcode::MoveTo));
    DecomposeContext::pushInt16(*ctx->commandStream, x);
    DecomposeContext::pushInt16(*ctx->commandStream, y);
    return 0;
}

static int ebfLineTo(const FT_Vector* to, void* user) {
    auto* ctx = static_cast<DecomposeContext*>(user);
    int16_t x = static_cast<int16_t>(std::round(static_cast<float>(to->x) * ctx->scale));
    int16_t y = static_cast<int16_t>(std::round(static_cast<float>(to->y) * ctx->scale));

    ctx->commandStream->push_back(static_cast<uint8_t>(EbfOpcode::LineTo));
    DecomposeContext::pushInt16(*ctx->commandStream, x);
    DecomposeContext::pushInt16(*ctx->commandStream, y);
    return 0;
}

static int ebfConicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
    auto* ctx = static_cast<DecomposeContext*>(user);
    int16_t cx = static_cast<int16_t>(std::round(static_cast<float>(control->x) * ctx->scale));
    int16_t cy = static_cast<int16_t>(std::round(static_cast<float>(control->y) * ctx->scale));
    int16_t x  = static_cast<int16_t>(std::round(static_cast<float>(to->x) * ctx->scale));
    int16_t y  = static_cast<int16_t>(std::round(static_cast<float>(to->y) * ctx->scale));

    ctx->commandStream->push_back(static_cast<uint8_t>(EbfOpcode::QuadTo));
    DecomposeContext::pushInt16(*ctx->commandStream, cx);
    DecomposeContext::pushInt16(*ctx->commandStream, cy);
    DecomposeContext::pushInt16(*ctx->commandStream, x);
    DecomposeContext::pushInt16(*ctx->commandStream, y);
    return 0;
}

static int ebfCubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
    auto* ctx = static_cast<DecomposeContext*>(user);
    int16_t c1x = static_cast<int16_t>(std::round(static_cast<float>(control1->x) * ctx->scale));
    int16_t c1y = static_cast<int16_t>(std::round(static_cast<float>(control1->y) * ctx->scale));
    int16_t c2x = static_cast<int16_t>(std::round(static_cast<float>(control2->x) * ctx->scale));
    int16_t c2y = static_cast<int16_t>(std::round(static_cast<float>(control2->y) * ctx->scale));
    int16_t x   = static_cast<int16_t>(std::round(static_cast<float>(to->x) * ctx->scale));
    int16_t y   = static_cast<int16_t>(std::round(static_cast<float>(to->y) * ctx->scale));

    ctx->commandStream->push_back(static_cast<uint8_t>(EbfOpcode::CubicTo));
    DecomposeContext::pushInt16(*ctx->commandStream, c1x);
    DecomposeContext::pushInt16(*ctx->commandStream, c1y);
    DecomposeContext::pushInt16(*ctx->commandStream, c2x);
    DecomposeContext::pushInt16(*ctx->commandStream, c2y);
    DecomposeContext::pushInt16(*ctx->commandStream, x);
    DecomposeContext::pushInt16(*ctx->commandStream, y);
    return 0;
}

// ---------------------------------------------------------------------------
// 2D Contour Triangulation with Hole Bridge Connection (Ear-Clipping)
// ---------------------------------------------------------------------------

struct Vec2 {
    float x{0.0f}, y{0.0f};
};

struct CollectorContext {
    std::vector<std::vector<Vec2>> contours;
    float scale{1.0f};
};

static int collectMoveTo(const FT_Vector* to, void* user) {
    auto* ctx = static_cast<CollectorContext*>(user);
    ctx->contours.emplace_back();
    ctx->contours.back().push_back({
        static_cast<float>(to->x) * ctx->scale,
        static_cast<float>(to->y) * ctx->scale
    });
    return 0;
}

static int collectLineTo(const FT_Vector* to, void* user) {
    auto* ctx = static_cast<CollectorContext*>(user);
    if (ctx->contours.empty()) ctx->contours.emplace_back();
    ctx->contours.back().push_back({
        static_cast<float>(to->x) * ctx->scale,
        static_cast<float>(to->y) * ctx->scale
    });
    return 0;
}

static int collectConicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
    auto* ctx = static_cast<CollectorContext*>(user);
    if (ctx->contours.empty()) ctx->contours.emplace_back();
    auto& pts = ctx->contours.back();
    if (pts.empty()) pts.push_back({0.0f, 0.0f});
    Vec2 p0 = pts.back();
    Vec2 p1 = { static_cast<float>(control->x) * ctx->scale, static_cast<float>(control->y) * ctx->scale };
    Vec2 p2 = { static_cast<float>(to->x) * ctx->scale, static_cast<float>(to->y) * ctx->scale };

    for (int step = 1; step <= 4; ++step) {
        float t = static_cast<float>(step) * 0.25f;
        float invT = 1.0f - t;
        float qx = invT * invT * p0.x + 2.0f * invT * t * p1.x + t * t * p2.x;
        float qy = invT * invT * p0.y + 2.0f * invT * t * p1.y + t * t * p2.y;
        pts.push_back({qx, qy});
    }
    return 0;
}

static int collectCubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
    auto* ctx = static_cast<CollectorContext*>(user);
    if (ctx->contours.empty()) ctx->contours.emplace_back();
    auto& pts = ctx->contours.back();
    if (pts.empty()) pts.push_back({0.0f, 0.0f});
    Vec2 p0 = pts.back();
    Vec2 p1 = { static_cast<float>(control1->x) * ctx->scale, static_cast<float>(control1->y) * ctx->scale };
    Vec2 p2 = { static_cast<float>(control2->x) * ctx->scale, static_cast<float>(control2->y) * ctx->scale };
    Vec2 p3 = { static_cast<float>(to->x) * ctx->scale, static_cast<float>(to->y) * ctx->scale };

    for (int step = 1; step <= 5; ++step) {
        float t = static_cast<float>(step) * 0.2f;
        float invT = 1.0f - t;
        float bx = invT * invT * invT * p0.x +
                   3.0f * invT * invT * t * p1.x +
                   3.0f * invT * t * t * p2.x +
                   t * t * t * p3.x;
        float by = invT * invT * invT * p0.y +
                   3.0f * invT * invT * t * p1.y +
                   3.0f * invT * t * t * p2.y +
                   t * t * t * p3.y;
        pts.push_back({bx, by});
    }
    return 0;
}

inline float polygonArea(const std::vector<Vec2>& poly) {
    if (poly.size() < 3) return 0.0f;
    float area = 0.0f;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    return area * 0.5f;
}

inline float cross2D(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

inline bool pointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) {
    auto sign = [](const Vec2& p1, const Vec2& p2, const Vec2& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    float d1 = sign(p, a, b);
    float d2 = sign(p, b, c);
    float d3 = sign(p, c, a);
    bool has_neg = (d1 < -1e-5f) || (d2 < -1e-5f) || (d3 < -1e-5f);
    bool has_pos = (d1 > 1e-5f) || (d2 > 1e-5f) || (d3 > 1e-5f);
    return !(has_neg && has_pos);
}

inline bool pointInPolygon(const Vec2& pt, const std::vector<Vec2>& poly) {
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
            (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y + 1e-9f) + poly[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

inline bool segmentsCross(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
    auto ccw = [](const Vec2& p1, const Vec2& p2, const Vec2& p3) {
        return (p3.y - p1.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p3.x - p1.x);
    };
    float c1 = ccw(a, b, c);
    float c2 = ccw(a, b, d);
    float c3 = ccw(c, d, a);
    float c4 = ccw(c, d, b);
    if (((c1 > 1e-4f && c2 < -1e-4f) || (c1 < -1e-4f && c2 > 1e-4f)) &&
        ((c3 > 1e-4f && c4 < -1e-4f) || (c3 < -1e-4f && c4 > 1e-4f))) {
        return true;
    }
    return false;
}

inline void earClip(std::vector<Vec2> poly, std::vector<Vec2>& outTriangles) {
    std::vector<Vec2> clean;
    clean.reserve(poly.size());
    for (const auto& pt : poly) {
        if (clean.empty() || std::hypot(clean.back().x - pt.x, clean.back().y - pt.y) > 0.5f) {
            clean.push_back(pt);
        }
    }
    if (clean.size() > 1 && std::hypot(clean.front().x - clean.back().x, clean.front().y - clean.back().y) <= 0.5f) {
        clean.pop_back();
    }
    if (clean.size() < 3) return;

    if (polygonArea(clean) < 0.0f) {
        std::reverse(clean.begin(), clean.end());
    }

    std::vector<int> V(clean.size());
    for (size_t i = 0; i < clean.size(); ++i) V[i] = static_cast<int>(i);

    int count = static_cast<int>(clean.size());
    int maxIters = count * 6;

    while (count > 2 && maxIters-- > 0) {
        int bestVi = -1;
        float bestQuality = -1e9f;

        for (int vi = 0; vi < count; ++vi) {
            int prev = V[(vi + count - 1) % count];
            int curr = V[vi];
            int next = V[(vi + 1) % count];

            const Vec2& a = clean[prev];
            const Vec2& b = clean[curr];
            const Vec2& c = clean[next];

            float cp = cross2D(a, b, c);
            if (cp <= 1e-5f) continue;

            // Centroid must lie strictly inside the polygon
            Vec2 centroid{(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f};
            if (!pointInPolygon(centroid, clean)) continue;

            // Chord a-c must not cross any other edge of the remaining polygon
            bool crosses = false;
            for (int e = 0; e < count; ++e) {
                int e1 = V[e];
                int e2 = V[(e + 1) % count];
                if (e1 == prev || e1 == curr || e1 == next || e2 == prev || e2 == curr || e2 == next) continue;
                const Vec2& pe1 = clean[e1];
                const Vec2& pe2 = clean[e2];
                // Skip coincident vertices
                if (std::abs(pe1.x - pe2.x) < 1e-3f && std::abs(pe1.y - pe2.y) < 1e-3f) continue;
                if (segmentsCross(a, c, pe1, pe2)) {
                    crosses = true;
                    break;
                }
            }
            if (crosses) continue;

            bool inside = false;
            for (int k = 0; k < count; ++k) {
                int vk = V[k];
                if (vk == prev || vk == curr || vk == next) continue;
                const Vec2& pk = clean[vk];
                // Skip coincident duplicate bridge vertices
                if ((std::abs(pk.x - a.x) < 1e-3f && std::abs(pk.y - a.y) < 1e-3f) ||
                    (std::abs(pk.x - b.x) < 1e-3f && std::abs(pk.y - b.y) < 1e-3f) ||
                    (std::abs(pk.x - c.x) < 1e-3f && std::abs(pk.y - c.y) < 1e-3f)) {
                    continue;
                }
                if (pointInTriangle(pk, a, b, c)) {
                    inside = true;
                    break;
                }
            }
            if (!inside) {
                // Quality metric: Area / (sum of squared edge lengths)
                // Maximizing this yields equilateral, fat triangles and eliminates needle slivers
                float d0 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
                float d1 = (c.x - b.x) * (c.x - b.x) + (c.y - b.y) * (c.y - b.y);
                float d2 = (a.x - c.x) * (a.x - c.x) + (a.y - c.y) * (a.y - c.y);
                float quality = cp / (d0 + d1 + d2 + 1e-4f);
                if (quality > bestQuality) {
                    bestQuality = quality;
                    bestVi = vi;
                }
            }
        }

        bool earFound = false;
        if (bestVi >= 0) {
            int prev = V[(bestVi + count - 1) % count];
            int curr = V[bestVi];
            int next = V[(bestVi + 1) % count];
            outTriangles.push_back(clean[prev]);
            outTriangles.push_back(clean[curr]);
            outTriangles.push_back(clean[next]);
            V.erase(V.begin() + bestVi);
            count--;
            earFound = true;
        }
        if (!earFound) {
            // Safe fallback: clip any convex triplet whose centroid is inside the polygon (never fan across holes!)
            for (int vi = 0; vi < count; ++vi) {
                int prev = V[(vi + count - 1) % count];
                int curr = V[vi];
                int next = V[(vi + 1) % count];
                const Vec2& a = clean[prev];
                const Vec2& b = clean[curr];
                const Vec2& c = clean[next];
                if (cross2D(a, b, c) > 1e-5f) {
                    Vec2 mid{(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f};
                    if (pointInPolygon(mid, clean)) {
                        outTriangles.push_back(a);
                        outTriangles.push_back(b);
                        outTriangles.push_back(c);
                        V.erase(V.begin() + vi);
                        count--;
                        earFound = true;
                        break;
                    }
                }
            }
            if (!earFound) break;
        }
    }
}

inline void triangulateGlyphContours(const std::vector<std::vector<Vec2>>& rawContours, std::vector<Vec2>& outTriangles) {
    if (rawContours.empty()) return;

    struct OuterPoly {
        std::vector<Vec2> outer;
        std::vector<std::vector<Vec2>> holes;
    };

    std::vector<OuterPoly> outers;
    std::vector<std::vector<Vec2>> holes;

    float totalPosArea = 0.0f;
    float totalNegArea = 0.0f;
    for (const auto& c : rawContours) {
        float a = polygonArea(c);
        if (a > 0.0f) totalPosArea += a;
        else if (a < 0.0f) totalNegArea += -a;
    }
    bool outerIsPositive = (totalPosArea >= totalNegArea);

    for (const auto& c : rawContours) {
        if (c.size() < 3) continue;
        float a = polygonArea(c);
        if (std::abs(a) < 1.0f) continue;

        bool isOuter = outerIsPositive ? (a > 0.0f) : (a < 0.0f);
        if (isOuter) {
            OuterPoly op;
            op.outer = c;
            if (polygonArea(op.outer) < 0.0f) {
                std::reverse(op.outer.begin(), op.outer.end());
            }
            outers.push_back(std::move(op));
        } else {
            std::vector<Vec2> h = c;
            if (polygonArea(h) > 0.0f) {
                std::reverse(h.begin(), h.end());
            }
            holes.push_back(std::move(h));
        }
    }

    if (outers.empty() && !holes.empty()) {
        for (auto& h : holes) {
            OuterPoly op;
            op.outer = std::move(h);
            if (polygonArea(op.outer) < 0.0f) std::reverse(op.outer.begin(), op.outer.end());
            outers.push_back(std::move(op));
        }
        holes.clear();
    }

    for (auto& h : holes) {
        if (h.empty()) continue;
        int bestIdx = -1;
        for (size_t i = 0; i < outers.size(); ++i) {
            if (pointInPolygon(h[0], outers[i].outer)) {
                bestIdx = static_cast<int>(i);
                break;
            }
        }
        if (bestIdx >= 0) {
            outers[bestIdx].holes.push_back(std::move(h));
        } else if (!outers.empty()) {
            outers[0].holes.push_back(std::move(h));
        }
    }

    auto subdivideContour = [](const std::vector<Vec2>& poly, float maxLen = 75.0f) {
        std::vector<Vec2> res;
        res.reserve(poly.size() * 2);
        size_t n = poly.size();
        for (size_t i = 0; i < n; ++i) {
            const Vec2& p1 = poly[i];
            const Vec2& p2 = poly[(i + 1) % n];
            res.push_back(p1);
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float len = std::hypot(dx, dy);
            if (len > maxLen) {
                int steps = static_cast<int>(std::ceil(len / maxLen));
                for (int s = 1; s < steps; ++s) {
                    float t = static_cast<float>(s) / static_cast<float>(steps);
                    res.push_back({p1.x + dx * t, p1.y + dy * t});
                }
            }
        }
        return res;
    };

    for (auto& op : outers) {
        if (op.outer.size() < 3) continue;

        // Subdivide outer contour so edge lengths never exceed ~75 font units (~1 pixel at 13px)
        // This ensures solid, unbroken geometry for single-contour glyphs (like 'C', 'S', 'E', 'F', 'T', 'Z')
        op.outer = subdivideContour(op.outer, 75.0f);

        if (op.holes.empty()) {
            earClip(op.outer, outTriangles);
            continue;
        }

        // Subdivide all hole contours as well so inner edges match the outer mesh resolution
        for (auto& h : op.holes) {
            h = subdivideContour(h, 75.0f);
        }

        // Sort holes top-to-bottom (Y descending) so multi-hole glyphs (like 'B' or '8') bridge cleanly without self-intersection
        std::sort(op.holes.begin(), op.holes.end(), [](const std::vector<Vec2>& ha, const std::vector<Vec2>& hb) {
            float avgYa = 0.0f, avgYb = 0.0f;
            for (const auto& p : ha) avgYa += p.y;
            for (const auto& p : hb) avgYb += p.y;
            return (avgYa / static_cast<float>(ha.size())) > (avgYb / static_cast<float>(hb.size()));
        });

        for (const auto& h : op.holes) {
            if (h.size() < 3) continue;

            size_t bestOuter = 0;
            size_t bestHIdx = 0;
            float minCost = 1e9f;

            // Search for optimal unobstructed bridge vertex pair
            for (size_t hIdx = 0; hIdx < h.size(); ++hIdx) {
                const Vec2& hp = h[hIdx];
                for (size_t i = 0; i < op.outer.size(); ++i) {
                    const Vec2& opPt = op.outer[i];
                    float dx = opPt.x - hp.x;
                    float dy = opPt.y - hp.y;
                    float d = std::hypot(dx, dy);
                    // Favor orthogonal horizontal bridges (low |dy|) across stems
                    float cost = d + std::abs(dy) * 2.0f;
                    // Strongly favor bridging leftward towards vertical stems (dx <= 1.0f)
                    // (protects curved outer loops in 'B', 'D', 'P', 'R' from being sliced by bridges)
                    if (dx <= 1.0f) {
                        cost *= 0.6f;
                    }
                    if (cost < minCost) {
                        bool blocked = false;
                        for (size_t j = 0; j < op.outer.size(); ++j) {
                            size_t nextJ = (j + 1) % op.outer.size();
                            if (j == i || nextJ == i) continue;
                            if (segmentsCross(hp, opPt, op.outer[j], op.outer[nextJ])) {
                                blocked = true;
                                break;
                            }
                        }
                        if (!blocked) {
                            for (size_t j = 0; j < h.size(); ++j) {
                                size_t nextJ = (j + 1) % h.size();
                                if (j == hIdx || nextJ == hIdx) continue;
                                if (segmentsCross(hp, opPt, h[j], h[nextJ])) {
                                    blocked = true;
                                    break;
                                }
                            }
                        }
                        if (!blocked) {
                            minCost = cost;
                            bestOuter = i;
                            bestHIdx = hIdx;
                        }
                    }
                }
            }

            if (minCost > 1e8f) {
                for (size_t hIdx = 0; hIdx < h.size(); ++hIdx) {
                    const Vec2& hp = h[hIdx];
                    for (size_t i = 0; i < op.outer.size(); ++i) {
                        float d = std::hypot(op.outer[i].x - hp.x, op.outer[i].y - hp.y);
                        if (d < minCost) {
                            minCost = d;
                            bestOuter = i;
                            bestHIdx = hIdx;
                        }
                    }
                }
            }

            std::vector<Vec2> spliced;
            spliced.reserve(op.outer.size() + h.size() + 2);
            for (size_t i = 0; i <= bestOuter; ++i) {
                spliced.push_back(op.outer[i]);
            }
            for (size_t i = 0; i < h.size(); ++i) {
                spliced.push_back(h[(bestHIdx + i) % h.size()]);
            }
            spliced.push_back(h[bestHIdx]);
            spliced.push_back(op.outer[bestOuter]);
            for (size_t i = bestOuter + 1; i < op.outer.size(); ++i) {
                spliced.push_back(op.outer[i]);
            }
            op.outer = std::move(spliced);
        }

        earClip(op.outer, outTriangles);
    }
}

} // anonymous namespace

std::vector<uint8_t> convertFtFace(FT_Face face, const EbfConversionOptions& options) {
    if (!face) return {};

    uint16_t targetEm = (options.targetUnitsPerEm > 0) ? options.targetUnitsPerEm : 1000;
    float fontEm = (face->units_per_EM > 0) ? static_cast<float>(face->units_per_EM) : 1000.0f;
    float scale = static_cast<float>(targetEm) / fontEm;

    EbfHeader header{};
    header.magic[0] = 'E';
    header.magic[1] = 'B';
    header.magic[2] = 'F';
    header.magic[3] = '1';
    header.version = 1;
    header.unitsPerEm = targetEm;
    header.ascender = static_cast<int16_t>(std::round(static_cast<float>(face->ascender) * scale));
    header.descender = static_cast<int16_t>(std::round(static_cast<float>(face->descender) * scale));
    header.lineGap = static_cast<int16_t>(std::round(static_cast<float>(face->height - (face->ascender - face->descender)) * scale));

    if (FT_IS_FIXED_WIDTH(face)) {
        header.flags |= 1; // Monospace
    }
    header.flags |= 2; // Solid filled vector glyphs with triangle geometry

    std::vector<uint32_t> codepoints;
    codepoints.reserve(256);

    for (uint32_t cp = options.firstCodepoint; cp <= options.lastCodepoint; ++cp) {
        codepoints.push_back(cp);
    }

    if (options.includeExtraLatin) {
        for (uint32_t cp = 0x00A0; cp <= 0x00FF; ++cp) {
            codepoints.push_back(cp);
        }
    }

    std::vector<EbfGlyphRecord> glyphs;
    std::vector<uint8_t> commands;
    int16_t maxAdvance = 0;
    int16_t capH = 0;
    int16_t xH = 0;

    FT_Outline_Funcs funcs{};
    funcs.move_to  = &ebfMoveTo;
    funcs.line_to  = &ebfLineTo;
    funcs.conic_to = &ebfConicTo;
    funcs.cubic_to = &ebfCubicTo;

    FT_Outline_Funcs collectFuncs{};
    collectFuncs.move_to  = &collectMoveTo;
    collectFuncs.line_to  = &collectLineTo;
    collectFuncs.conic_to = &collectConicTo;
    collectFuncs.cubic_to = &collectCubicTo;

    DecomposeContext ctx{&commands, scale};

    for (uint32_t cp : codepoints) {
        FT_UInt gIndex = FT_Get_Char_Index(face, cp);
        if (gIndex == 0 && cp != ' ') continue;

        if (FT_Load_Glyph(face, gIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0) {
            continue;
        }

        EbfGlyphRecord rec{};
        rec.codepoint = cp;
        rec.advanceX = static_cast<int16_t>(std::round(static_cast<float>(face->glyph->advance.x) * scale));
        rec.bearingX = static_cast<int16_t>(std::round(static_cast<float>(face->glyph->metrics.horiBearingX) * scale));
        rec.bearingY = static_cast<int16_t>(std::round(static_cast<float>(face->glyph->metrics.horiBearingY) * scale));
        rec.width    = static_cast<uint16_t>(std::max(0.0f, std::round(static_cast<float>(face->glyph->metrics.width) * scale)));
        rec.height   = static_cast<uint16_t>(std::max(0.0f, std::round(static_cast<float>(face->glyph->metrics.height) * scale)));

        if (rec.advanceX > maxAdvance) maxAdvance = rec.advanceX;
        if (cp == 'H' || cp == 'T' || cp == 'E') capH = rec.bearingY;
        if (cp == 'x' || cp == 'o') xH = rec.bearingY;

        rec.commandOffset = static_cast<uint32_t>(commands.size());

        if (face->glyph->outline.n_contours > 0) {
            // 1. Decompose into closed polyline contours and triangulate into solid filled triangles
            CollectorContext collectCtx;
            collectCtx.scale = scale;
            FT_Outline_Decompose(&face->glyph->outline, &collectFuncs, &collectCtx);

            std::vector<Vec2> triangles;
            triangulateGlyphContours(collectCtx.contours, triangles);

            if (!triangles.empty()) {
                for (size_t t = 0; t + 2 < triangles.size(); t += 3) {
                    commands.push_back(static_cast<uint8_t>(EbfOpcode::Triangle));
                    DecomposeContext::pushInt16(commands, static_cast<int16_t>(std::round(triangles[t].x)));
                    DecomposeContext::pushInt16(commands, static_cast<int16_t>(std::round(triangles[t].y)));
                    DecomposeContext::pushInt16(commands, static_cast<int16_t>(std::round(triangles[t + 1].x)));
                    DecomposeContext::pushInt16(commands, static_cast<int16_t>(std::round(triangles[t + 1].y)));
                    DecomposeContext::pushInt16(commands, static_cast<int16_t>(std::round(triangles[t + 2].x)));
                    DecomposeContext::pushInt16(commands, static_cast<int16_t>(std::round(triangles[t + 2].y)));
                }
            } else {
                // Fallback to perimeter outlines only if no solid triangles were generated
                FT_Outline_Decompose(&face->glyph->outline, &funcs, &ctx);
            }
            commands.push_back(static_cast<uint8_t>(EbfOpcode::End));
        }

        rec.commandLength = static_cast<uint16_t>(commands.size() - rec.commandOffset);
        glyphs.push_back(rec);
    }

    header.glyphCount = static_cast<uint16_t>(glyphs.size());
    header.maxAdvance = maxAdvance;
    header.capHeight = (capH > 0) ? capH : static_cast<int16_t>(std::round(static_cast<float>(header.ascender) * 0.9f));
    header.xHeight = (xH > 0) ? xH : static_cast<int16_t>(std::round(static_cast<float>(header.capHeight) * 0.7f));

    // Optional Kerning
    std::vector<EbfKerningPair> kerningPairs;
    if (options.includeKerning && FT_HAS_KERNING(face)) {
        for (const auto& gLeft : glyphs) {
            FT_UInt idxLeft = FT_Get_Char_Index(face, gLeft.codepoint);
            if (idxLeft == 0) continue;

            for (const auto& gRight : glyphs) {
                FT_UInt idxRight = FT_Get_Char_Index(face, gRight.codepoint);
                if (idxRight == 0) continue;

                FT_Vector kern{};
                FT_Get_Kerning(face, idxLeft, idxRight, FT_KERNING_UNSCALED, &kern);
                if (kern.x != 0) {
                    EbfKerningPair kp{};
                    kp.leftCodepoint = gLeft.codepoint;
                    kp.rightCodepoint = gRight.codepoint;
                    kp.kerningAmount = static_cast<int16_t>(std::round(static_cast<float>(kern.x) * scale));
                    kerningPairs.push_back(kp);
                }
            }
        }
    }

    if (!kerningPairs.empty()) {
        header.flags |= (1 << 2); // Has kerning
    }

    // Assemble final binary buffer
    size_t totalSize = sizeof(EbfHeader) +
                       glyphs.size() * sizeof(EbfGlyphRecord) +
                       commands.size() +
                       kerningPairs.size() * sizeof(EbfKerningPair);

    std::vector<uint8_t> binary(totalSize);
    std::memcpy(binary.data(), &header, sizeof(EbfHeader));
    size_t off = sizeof(EbfHeader);

    std::memcpy(binary.data() + off, glyphs.data(), glyphs.size() * sizeof(EbfGlyphRecord));
    off += glyphs.size() * sizeof(EbfGlyphRecord);

    if (!commands.empty()) {
        std::memcpy(binary.data() + off, commands.data(), commands.size());
        off += commands.size();
    }

    if (!kerningPairs.empty()) {
        std::memcpy(binary.data() + off, kerningPairs.data(), kerningPairs.size() * sizeof(EbfKerningPair));
    }

    return binary;
}

bool EbfConverter::convertFile(const std::string& ttfPath,
                               const std::string& ebfPath,
                               const EbfConversionOptions& options) {
    FT_Library ftLib = nullptr;
    if (FT_Init_FreeType(&ftLib) != 0) return false;

    FT_Face face = nullptr;
    if (FT_New_Face(ftLib, ttfPath.c_str(), 0, &face) != 0) {
        FT_Done_FreeType(ftLib);
        return false;
    }

    std::vector<uint8_t> binary = convertFtFace(face, options);
    FT_Done_Face(face);
    FT_Done_FreeType(ftLib);

    if (binary.empty()) return false;

    std::ofstream out(ebfPath, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    return out.good();
}


std::vector<uint8_t> EbfConverter::convertMemory(const uint8_t* ttfData,
                                                 size_t ttfSize,
                                                 const EbfConversionOptions& options) {
    if (!ttfData || ttfSize == 0) return {};

    FT_Library ftLib = nullptr;
    if (FT_Init_FreeType(&ftLib) != 0) return {};

    FT_Face face = nullptr;
    if (FT_New_Memory_Face(ftLib, ttfData, static_cast<FT_Long>(ttfSize), 0, &face) != 0) {
        FT_Done_FreeType(ftLib);
        return {};
    }

    std::vector<uint8_t> binary = convertFtFace(face, options);
    FT_Done_Face(face);
    FT_Done_FreeType(ftLib);

    return binary;
}

bool EbfConverter::loadTtfIntoFont(const std::string& ttfPath,
                                   EbfFont& outFont,
                                   const EbfConversionOptions& options) {
    FT_Library ftLib = nullptr;
    if (FT_Init_FreeType(&ftLib) != 0) return false;

    FT_Face face = nullptr;
    if (FT_New_Face(ftLib, ttfPath.c_str(), 0, &face) != 0) {
        FT_Done_FreeType(ftLib);
        return false;
    }

    std::vector<uint8_t> binary = convertFtFace(face, options);
    FT_Done_Face(face);
    FT_Done_FreeType(ftLib);

    if (binary.empty()) return false;
    return outFont.loadFromMemory(binary.data(), binary.size());
}

bool EbfConverter::loadTtfMemoryIntoFont(const uint8_t* ttfData,
                                         size_t ttfSize,
                                         EbfFont& outFont,
                                         const EbfConversionOptions& options) {
    std::vector<uint8_t> binary = convertMemory(ttfData, ttfSize, options);
    if (binary.empty()) return false;
    return outFont.loadFromMemory(binary.data(), binary.size());
}

bool EbfConverter::convertToCppHeader(const std::string& ttfPath,
                                      const std::string& headerPath,
                                      const std::string& fontVarName,
                                      const EbfConversionOptions& options) {
    FT_Library ftLib = nullptr;
    if (FT_Init_FreeType(&ftLib) != 0) return false;

    FT_Face face = nullptr;
    if (FT_New_Face(ftLib, ttfPath.c_str(), 0, &face) != 0) {
        FT_Done_FreeType(ftLib);
        return false;
    }

    std::vector<uint8_t> binary = convertFtFace(face, options);
    FT_Done_Face(face);
    FT_Done_FreeType(ftLib);

    if (binary.empty()) return false;

    std::ofstream out(headerPath);
    if (!out.is_open()) return false;

    out << "#pragma once\n\n";
    out << "#include <cstdint>\n#include <cstddef>\n\n";
    out << "// Auto-generated by Eatsbits EbfConverter from " << ttfPath << "\n";
    out << "// Total size: " << binary.size() << " bytes\n";
    out << "inline constexpr size_t k" << fontVarName << "Size = " << binary.size() << ";\n";
    out << "inline constexpr uint8_t k" << fontVarName << "Data[] = {\n";

    for (size_t i = 0; i < binary.size(); ++i) {
        if (i % 16 == 0) out << "    ";
        out << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(binary[i]) << ", ";
        if ((i + 1) % 16 == 0) out << "\n";
    }
    out << "\n};\n";
    return out.good();
}

} // namespace eatsbits::ui::glyph
