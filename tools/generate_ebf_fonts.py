#!/usr/bin/env python3
"""
tools/generate_ebf_fonts.py
Pre-converts TTF fonts into optimized .ebf (Eatsbits Font) binary files.
Features:
- Exact FreeType quadratic/cubic spline decomposition
- Multi-hole top-to-bottom bridging with stem-preference weighting
- Collinear straight-edge subdivision preventing diagonal hypotenuses on thin stems
- Quality-metric fat-triangle ear-clipping
"""

import freetype
import math
import struct
import os
import sys

class Vec2:
    __slots__ = ('x', 'y')
    def __init__(self, x, y):
        self.x = float(x)
        self.y = float(y)
    def __repr__(self):
        return f"({self.x:.1f}, {self.y:.1f})"

def polygon_area(poly):
    if len(poly) < 3: return 0.0
    area = 0.0
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        area += poly[i].x * poly[j].y - poly[j].x * poly[i].y
    return area * 0.5

def point_in_polygon(pt, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) and
            (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y + 1e-9) + poly[i].x)):
            inside = not inside
        j = i
    return inside

def segments_cross(a, b, c, d):
    def ccw(p1, p2, p3):
        return (p3.y - p1.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p3.x - p1.x)
    c1 = ccw(a, b, c)
    c2 = ccw(a, b, d)
    c3 = ccw(c, d, a)
    c4 = ccw(c, d, b)
    if (((c1 > 1e-4 and c2 < -1e-4) or (c1 < -1e-4 and c2 > 1e-4)) and
        ((c3 > 1e-4 and c4 < -1e-4) or (c3 < -1e-4 and c4 > 1e-4))):
        return True
    return False

def point_in_triangle(p, a, b, c):
    def sign(p1, p2, p3):
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y)
    d1 = sign(p, a, b)
    d2 = sign(p, b, c)
    d3 = sign(p, c, a)
    has_neg = (d1 < -1e-5) or (d2 < -1e-5) or (d3 < -1e-5)
    has_pos = (d1 > 1e-5) or (d2 > 1e-5) or (d3 > 1e-5)
    return not (has_neg and has_pos)

def ear_clip(poly):
    clean = []
    for pt in poly:
        if not clean or math.hypot(clean[-1].x - pt.x, clean[-1].y - pt.y) > 0.5:
            clean.append(pt)
    if len(clean) > 1 and math.hypot(clean[0].x - clean[-1].x, clean[0].y - clean[-1].y) <= 0.5:
        clean.pop()
    if len(clean) < 3:
        return []
    if polygon_area(clean) < 0.0:
        clean.reverse()

    V = list(range(len(clean)))
    count = len(clean)
    max_iters = count * 6
    out_triangles = []

    while count > 2 and max_iters > 0:
        max_iters -= 1
        best_vi = -1
        best_quality = -1e9

        for vi in range(count):
            prev = V[(vi + count - 1) % count]
            curr = V[vi]
            nxt  = V[(vi + 1) % count]
            a, b, c = clean[prev], clean[curr], clean[nxt]
            cp = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
            if cp <= 1e-5:
                continue

            centroid = Vec2((a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0)
            if not point_in_polygon(centroid, clean):
                continue

            crosses = False
            for e in range(count):
                e1 = V[e]
                e2 = V[(e + 1) % count]
                if e1 in (prev, curr, nxt) or e2 in (prev, curr, nxt):
                    continue
                pe1, pe2 = clean[e1], clean[e2]
                if abs(pe1.x - pe2.x) < 1e-3 and abs(pe1.y - pe2.y) < 1e-3:
                    continue
                if segments_cross(a, c, pe1, pe2):
                    crosses = True
                    break
            if crosses:
                continue

            inside = False
            for k in range(count):
                vk = V[k]
                if vk in (prev, curr, nxt):
                    continue
                pk = clean[vk]
                if ((abs(pk.x - a.x) < 1e-3 and abs(pk.y - a.y) < 1e-3) or
                    (abs(pk.x - b.x) < 1e-3 and abs(pk.y - b.y) < 1e-3) or
                    (abs(pk.x - c.x) < 1e-3 and abs(pk.y - c.y) < 1e-3)):
                    continue
                if point_in_triangle(pk, a, b, c):
                    inside = True
                    break
            if not inside:
                d0 = (b.x - a.x)**2 + (b.y - a.y)**2
                d1 = (c.x - b.x)**2 + (c.y - b.y)**2
                d2 = (a.x - c.x)**2 + (a.y - c.y)**2
                quality = cp / (d0 + d1 + d2 + 1e-4)
                if quality > best_quality:
                    best_quality = quality
                    best_vi = vi

        if best_vi >= 0:
            prev = V[(best_vi + count - 1) % count]
            curr = V[best_vi]
            nxt  = V[(best_vi + 1) % count]
            out_triangles.extend([clean[prev], clean[curr], clean[nxt]])
            del V[best_vi]
            count -= 1
        else:
            for vi in range(count):
                prev = V[(vi + count - 1) % count]
                curr = V[vi]
                nxt  = V[(vi + 1) % count]
                a, b, c = clean[prev], clean[curr], clean[nxt]
                cp = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
                if cp > 1e-5:
                    mid = Vec2((a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0)
                    if point_in_polygon(mid, clean):
                        out_triangles.extend([a, b, c])
                        del V[vi]
                        count -= 1
                        break
            else:
                break
    return out_triangles

def triangulate_glyph_contours(raw_contours):
    if not raw_contours:
        return []

    total_pos = sum(polygon_area(c) for c in raw_contours if polygon_area(c) > 0)
    total_neg = sum(-polygon_area(c) for c in raw_contours if polygon_area(c) < 0)
    outer_is_pos = (total_pos >= total_neg)

    outers = []
    holes = []
    for c in raw_contours:
        if len(c) < 3: continue
        a = polygon_area(c)
        if abs(a) < 1.0: continue
        is_outer = (a > 0) if outer_is_pos else (a < 0)
        poly = list(c)
        if is_outer:
            if polygon_area(poly) < 0: poly.reverse()
            outers.append({'outer': poly, 'holes': []})
        else:
            if polygon_area(poly) > 0: poly.reverse()
            holes.append(poly)

    if not outers and holes:
        for h in holes:
            if polygon_area(h) < 0: h.reverse()
            outers.append({'outer': h, 'holes': []})
        holes = []

    for h in holes:
        if not h: continue
        best_idx = -1
        for i, op in enumerate(outers):
            if point_in_polygon(h[0], op['outer']):
                best_idx = i
                break
        if best_idx >= 0:
            outers[best_idx]['holes'].append(h)
        elif outers:
            outers[0]['holes'].append(h)

    def subdivide_contour(poly, max_len=75.0):
        res = []
        n = len(poly)
        for i in range(n):
            p1 = poly[i]
            p2 = poly[(i + 1) % n]
            res.append(p1)
            dx = p2.x - p1.x
            dy = p2.y - p1.y
            l = math.hypot(dx, dy)
            if l > max_len:
                steps = int(math.ceil(l / max_len))
                for s in range(1, steps):
                    t = float(s) / float(steps)
                    res.append(Vec2(p1.x + dx * t, p1.y + dy * t))
        return res

    out_triangles = []
    for op in outers:
        if len(op['outer']) < 3:
            continue

        # Subdivide all outer contours so edge lengths never exceed ~75 font units (~1 pixel at 13px)
        # This prevents giant diagonal slivers and breaks in single-contour glyphs (like 'C', 'S', 'E', 'F', 'T', 'Z')
        op['outer'] = subdivide_contour(op['outer'], 75.0)

        if not op['holes']:
            out_triangles.extend(ear_clip(op['outer']))
            continue

        # Subdivide all hole contours so inner edges match the mesh resolution (fixes 'D', 'O', 'A', '8', etc.)
        for h_idx in range(len(op['holes'])):
            op['holes'][h_idx] = subdivide_contour(op['holes'][h_idx], 75.0)

        poly = list(op['outer'])

        # Sort holes top-to-bottom (Y descending)
        op['holes'].sort(key=lambda h: sum(p.y for p in h) / len(h), reverse=True)

        for h in op['holes']:
            if len(h) < 3: continue
            best_outer = 0
            best_h_idx = 0
            min_cost = 1e9

            for h_idx in range(len(h)):
                hp = h[h_idx]
                for i in range(len(poly)):
                    op_pt = poly[i]
                    dx = op_pt.x - hp.x
                    dy = op_pt.y - hp.y
                    d = math.hypot(dx, dy)
                    cost = d + abs(dy) * 2.0
                    if dx <= 1.0:
                        cost *= 0.6
                    if cost < min_cost:
                        blocked = False
                        for j in range(len(poly)):
                            nj = (j + 1) % len(poly)
                            if j == i or nj == i: continue
                            if segments_cross(hp, op_pt, poly[j], poly[nj]):
                                blocked = True
                                break
                        if not blocked:
                            for j in range(len(h)):
                                nj = (j + 1) % len(h)
                                if j == h_idx or nj == h_idx: continue
                                if segments_cross(hp, op_pt, h[j], h[nj]):
                                    blocked = True
                                    break
                        if not blocked:
                            min_cost = cost
                            best_outer = i
                            best_h_idx = h_idx

            if min_cost > 1e8:
                for h_idx in range(len(h)):
                    hp = h[h_idx]
                    for i in range(len(poly)):
                        d = math.hypot(poly[i].x - hp.x, poly[i].y - hp.y)
                        if d < min_cost:
                            min_cost = d
                            best_outer = i
                            best_h_idx = h_idx

            spliced = (poly[:best_outer + 1] +
                       [h[(best_h_idx + k) % len(h)] for k in range(len(h))] +
                       [h[best_h_idx], poly[best_outer]] +
                       poly[best_outer + 1:])
            poly = spliced

        out_triangles.extend(ear_clip(poly))

    return out_triangles

def convert_ttf_to_ebf(ttf_path, ebf_path, target_em=1000):
    print(f"[*] Converting '{ttf_path}' -> '{ebf_path}' (targetUnitsPerEm={target_em})...")
    face = freetype.Face(ttf_path)
    font_em = float(face.units_per_EM) if face.units_per_EM > 0 else 1000.0
    scale = float(target_em) / font_em

    flags = 2 # Filled polygon triangles
    if face.is_fixed_width:
        flags |= 1 # Monospace

    ascender = int(round(float(face.ascender) * scale))
    descender = int(round(float(face.descender) * scale))
    line_gap = int(round(float(face.height - (face.ascender - face.descender)) * scale))

    codepoints = list(range(0x0020, 0x007F)) + list(range(0x00A0, 0x0100))

    glyphs = []
    commands = bytearray()
    max_advance = 0
    cap_h = 0
    x_h = 0

    for cp in codepoints:
        g_index = face.get_char_index(cp)
        if g_index == 0 and cp != ord(' '):
            continue

        try:
            face.load_glyph(g_index, freetype.FT_LOAD_NO_SCALE | freetype.FT_LOAD_NO_BITMAP | freetype.FT_LOAD_NO_HINTING)
        except Exception:
            continue

        glyph = face.glyph
        adv_x = int(round(float(glyph.advance.x) * scale))
        bear_x = int(round(float(glyph.metrics.horiBearingX) * scale))
        bear_y = int(round(float(glyph.metrics.horiBearingY) * scale))
        w = max(0, int(round(float(glyph.metrics.width) * scale)))
        h = max(0, int(round(float(glyph.metrics.height) * scale)))

        if adv_x > max_advance: max_advance = adv_x
        if cp in (ord('H'), ord('T'), ord('E')): cap_h = bear_y
        if cp in (ord('x'), ord('o')): x_h = bear_y

        cmd_offset = len(commands)

        if glyph.outline.n_contours > 0:
            contours = []
            def move_to(to, user):
                contours.append([Vec2(to.x * scale, to.y * scale)])
                return 0
            def line_to(to, user):
                contours[-1].append(Vec2(to.x * scale, to.y * scale))
                return 0
            def conic_to(control, to, user):
                p0 = contours[-1][-1]
                p1 = Vec2(control.x * scale, control.y * scale)
                p2 = Vec2(to.x * scale, to.y * scale)
                for step in range(1, 5):
                    t = step * 0.25
                    inv_t = 1.0 - t
                    qx = inv_t * inv_t * p0.x + 2.0 * inv_t * t * p1.x + t * t * p2.x
                    qy = inv_t * inv_t * p0.y + 2.0 * inv_t * t * p1.y + t * t * p2.y
                    contours[-1].append(Vec2(qx, qy))
                return 0
            def cubic_to(c1, c2, to, user):
                p0 = contours[-1][-1]
                p1 = Vec2(c1.x * scale, c1.y * scale)
                p2 = Vec2(c2.x * scale, c2.y * scale)
                p3 = Vec2(to.x * scale, to.y * scale)
                for step in range(1, 7):
                    t = step / 6.0
                    inv_t = 1.0 - t
                    bx = inv_t**3 * p0.x + 3.0 * inv_t**2 * t * p1.x + 3.0 * inv_t * t**2 * p2.x + t**3 * p3.x
                    by = inv_t**3 * p0.y + 3.0 * inv_t**2 * t * p1.y + 3.0 * inv_t * t**2 * p2.y + t**3 * p3.y
                    contours[-1].append(Vec2(bx, by))
                return 0

            glyph.outline.decompose(context=None, move_to=move_to, line_to=line_to, conic_to=conic_to, cubic_to=cubic_to)
            triangles = triangulate_glyph_contours(contours)

            for t in range(0, len(triangles) - 2, 3):
                commands.append(8) # EbfOpcode::Triangle
                commands.extend(struct.pack('<hhhhhh',
                    int(round(triangles[t].x)),     int(round(triangles[t].y)),
                    int(round(triangles[t + 1].x)), int(round(triangles[t + 1].y)),
                    int(round(triangles[t + 2].x)), int(round(triangles[t + 2].y))))
            commands.append(0) # EbfOpcode::End

        cmd_len = len(commands) - cmd_offset
        glyphs.append({
            'cp': cp, 'adv_x': adv_x, 'bear_x': bear_x, 'bear_y': bear_y,
            'w': w, 'h': h, 'cmd_offset': cmd_offset, 'cmd_len': cmd_len, 'flags': 0
        })

    # Sort glyphs by codepoint
    glyphs.sort(key=lambda g: g['cp'])

    cap_h = cap_h if cap_h > 0 else int(round(ascender * 0.9))
    x_h = x_h if x_h > 0 else int(round(cap_h * 0.7))

    # Build Header (34 bytes: magic[4], ver, flags, upm, glyphCount, asc, desc, lineGap, capH, xH, maxAdv, reserved[10])
    hdr_bytes = struct.pack('<4sHHHHhhhhhh10s',
        b'EBF1', 1, flags, target_em, len(glyphs),
        ascender, descender, line_gap, cap_h, x_h, max_advance, b'\x00' * 10
    )

    # Build Glyph Table (22 bytes per record: cp(I), advX(h), bearX(h), bearY(h), w(H), h(H), cmdOffset(I), cmdLen(H), flags(H))
    table_bytes = bytearray()
    for g in glyphs:
        table_bytes.extend(struct.pack('<IhhhHHIHH',
            g['cp'], g['adv_x'], g['bear_x'], g['bear_y'],
            g['w'], g['h'], g['cmd_offset'], g['cmd_len'], g['flags']
        ))

    out_data = hdr_bytes + table_bytes + commands
    with open(ebf_path, 'wb') as f:
        f.write(out_data)

    print(f"[+] Successfully generated '{ebf_path}': {len(out_data)} bytes, {len(glyphs)} glyphs.")

if __name__ == '__main__':
    convert_ttf_to_ebf('assets/fonts/Oswald-Regular.ttf', 'assets/fonts/Oswald-Regular.ebf')
    convert_ttf_to_ebf('assets/fonts/ShareTechMono-Regular.ttf', 'assets/fonts/ShareTechMono-Regular.ebf')
