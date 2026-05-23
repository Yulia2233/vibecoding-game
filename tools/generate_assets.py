#!/usr/bin/env python3
import math
import os
import struct
import subprocess
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPRITES = ROOT / "assets" / "sprites"
AUDIO = ROOT / "assets" / "audio"


def ensure_dirs():
    SPRITES.mkdir(parents=True, exist_ok=True)
    AUDIO.mkdir(parents=True, exist_ok=True)


def rgb(hex_color):
    hex_color = hex_color.lstrip("#")
    return tuple(int(hex_color[i : i + 2], 16) for i in (0, 2, 4))


def blend(a, b, t):
    return tuple(int(a[i] * (1 - t) + b[i] * t) for i in range(3))


def write_ppm(path, w, h, pixels):
    ppm = path.with_suffix(".ppm")
    with ppm.open("wb") as f:
        f.write(f"P6\n{w} {h}\n255\n".encode())
        for p in pixels:
            f.write(bytes(p))
    subprocess.run(["sips", "-s", "format", "png", str(ppm), "--out", str(path)], check=True, stdout=subprocess.DEVNULL)
    ppm.unlink()


def ellipse_mask(x, y, cx, cy, rx, ry):
    if rx <= 0 or ry <= 0:
        return False
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def draw_disc(img, w, h, cx, cy, r, color):
    for y in range(max(0, int(cy - r - 1)), min(h, int(cy + r + 2))):
        for x in range(max(0, int(cx - r - 1)), min(w, int(cx + r + 2))):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                img[y * w + x] = color


def draw_ellipse(img, w, h, cx, cy, rx, ry, color):
    for y in range(max(0, int(cy - ry - 1)), min(h, int(cy + ry + 2))):
        for x in range(max(0, int(cx - rx - 1)), min(w, int(cx + rx + 2))):
            if ellipse_mask(x, y, cx, cy, rx, ry):
                img[y * w + x] = color


def draw_rect(img, w, h, x0, y0, x1, y1, color):
    for y in range(max(0, int(y0)), min(h, int(y1))):
        for x in range(max(0, int(x0)), min(w, int(x1))):
            img[y * w + x] = color


def draw_triangle(img, w, h, p0, p1, p2, color):
    minx = max(0, int(min(p0[0], p1[0], p2[0])))
    maxx = min(w - 1, int(max(p0[0], p1[0], p2[0])))
    miny = max(0, int(min(p0[1], p1[1], p2[1])))
    maxy = min(h - 1, int(max(p0[1], p1[1], p2[1])))
    def sign(p, a, b):
        return (p[0] - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (p[1] - b[1])
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            p = (x + 0.5, y + 0.5)
            d1 = sign(p, p0, p1)
            d2 = sign(p, p1, p2)
            d3 = sign(p, p2, p0)
            if not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0)):
                img[y * w + x] = color


def make_scene():
    w, h = 430, 900
    pixels = []
    top = rgb("#263b49")
    bot = rgb("#61717a")
    for y in range(h):
        t = y / (h - 1)
        base = blend(top, bot, t)
        for x in range(w):
            side = abs(x - w / 2) / (w / 2)
            cool = int(18 * side * (0.4 + 0.6 * t))
            pixels.append((max(0, base[0] - cool), max(0, base[1] + int(8 * side) - cool // 2), min(255, base[2] + int(14 * side))))

    def poly(points, color):
        # Scanline fill, enough for large decorative shapes.
        for y in range(h):
            nodes = []
            j = len(points) - 1
            for i, pi in enumerate(points):
                pj = points[j]
                if (pi[1] < y <= pj[1]) or (pj[1] < y <= pi[1]):
                    x = int(pi[0] + (y - pi[1]) / (pj[1] - pi[1]) * (pj[0] - pi[0]))
                    nodes.append(x)
                j = i
            nodes.sort()
            for i in range(0, len(nodes), 2):
                if i + 1 >= len(nodes):
                    break
                for x in range(max(0, nodes[i]), min(w, nodes[i + 1])):
                    pixels[y * w + x] = color

    draw_disc(pixels, w, h, 38, 92, 94, rgb("#223d47"))
    draw_disc(pixels, w, h, 388, 92, 105, rgb("#1d3b46"))
    draw_disc(pixels, w, h, 48, 560, 162, rgb("#b4cfda"))
    draw_disc(pixels, w, h, 390, 566, 170, rgb("#a5c9d5"))
    draw_disc(pixels, w, h, 32, 760, 130, rgb("#b8d4df"))
    draw_disc(pixels, w, h, 396, 756, 128, rgb("#afd0db"))
    path = [(116, 145), (314, 145), (350, 255), (342, 535), (372, 666), (58, 666), (90, 535), (82, 250)]
    poly(path, rgb("#a28d83"))
    inner = [(132, 174), (300, 174), (326, 268), (319, 528), (342, 638), (88, 638), (111, 528), (105, 268)]
    poly(inner, rgb("#b39c91"))
    for y in range(224, 630, 58):
        draw_rect(pixels, w, h, 92, y, 338, y + 1, rgb("#766c68"))
    for x in range(128, 320, 52):
        draw_rect(pixels, w, h, x, 202, x + 1, 640, rgb("#786d68"))
    for x in (18, 340):
        draw_rect(pixels, w, h, x, 98, x + 72, 176, rgb("#653f3b"))
        draw_rect(pixels, w, h, x, 128, x + 72, 190, rgb("#89373e"))
        draw_rect(pixels, w, h, x + 6, 88, x + 66, 112, rgb("#d7e7ea"))
    draw_rect(pixels, w, h, 0, 176, w, 195, rgb("#1e2b34"))
    for cx, cy, s in [(58, 208, 1), (372, 208, 1), (404, 520, 1)]:
        draw_rect(pixels, w, h, cx - 4 * s, cy + 7 * s, cx + 5 * s, cy + 52 * s, rgb("#4c3b31"))
        draw_disc(pixels, w, h, cx, cy, 20 * s, rgb("#e9883d"))
        draw_triangle(pixels, w, h, (cx, cy - 22 * s), (cx - 12 * s, cy + 9 * s), (cx + 10 * s, cy + 9 * s), rgb("#ffb23d"))
    write_ppm(SPRITES / "scene_valley.png", w, h, pixels)


def make_unit(path, main, trim, kind, frame):
    w, h = 96, 96
    bg = rgb("#00ff00")
    img = [bg] * (w * h)
    bob = -2 if frame == 1 else 1
    draw_ellipse(img, w, h, 48, 76, 28, 10, rgb("#1a242a"))
    if kind == "flyer":
        draw_triangle(img, w, h, (35, 40 + bob), (5, 28 + bob), (32, 54 + bob), blend(main, rgb("#ffffff"), 0.18))
        draw_triangle(img, w, h, (61, 40 + bob), (91, 28 + bob), (64, 54 + bob), blend(main, rgb("#ffffff"), 0.18))
    draw_disc(img, w, h, 48, 46 + bob, 27, rgb("#243f42"))
    draw_disc(img, w, h, 48, 42 + bob, 23, main)
    if kind == "boss":
        draw_disc(img, w, h, 48, 42 + bob, 31, rgb("#243f42"))
        draw_disc(img, w, h, 48, 39 + bob, 27, main)
    if kind == "hero":
        draw_triangle(img, w, h, (25, 24 + bob), (48, 5 + bob), (72, 24 + bob), trim)
    else:
        draw_triangle(img, w, h, (33, 28 + bob), (18, 12 + bob), (35, 42 + bob), rgb("#223d40"))
        draw_triangle(img, w, h, (63, 28 + bob), (78, 12 + bob), (61, 42 + bob), rgb("#223d40"))
    draw_disc(img, w, h, 39, 39 + bob, 4, rgb("#10252a"))
    draw_disc(img, w, h, 58, 39 + bob, 4, rgb("#10252a"))
    draw_rect(img, w, h, 39, 54 + bob, 58, 59 + bob, rgb("#f8f4dc"))
    if kind in ("brute", "boss"):
        draw_rect(img, w, h, 68, 52 + bob, 76, 86 + bob, rgb("#654232"))
        draw_disc(img, w, h, 74, 42 + bob, 14, rgb("#bdc8ce"))
    if kind == "shaman":
        draw_rect(img, w, h, 70, 40 + bob, 75, 82 + bob, rgb("#7b5037"))
        draw_disc(img, w, h, 72, 28 + bob, 8, trim)
    if kind == "hero":
        draw_rect(img, w, h, 24, 56 + bob, 30, 84 + bob, trim)
        draw_disc(img, w, h, 72, 54 + bob, 8, trim)
    write_ppm(path, w, h, img)


def make_sprites():
    heroes = [
        ("hero_archer", "#65a24d", "#e5ec9d"),
        ("hero_mage", "#7ed8e8", "#effcff"),
        ("hero_knight", "#c66f49", "#ffda5c"),
        ("hero_poison", "#8b63ba", "#caf47a"),
        ("hero_mechanic", "#6388bd", "#f0b64f"),
        ("hero_priest", "#b166c4", "#ffeb8a"),
    ]
    for name, main, trim in heroes:
        for frame in (0, 1):
            make_unit(SPRITES / f"{name}_{frame}.png", rgb(main), rgb(trim), "hero", frame)
    enemies = [
        ("enemy_scout", "#54b8ac", "#9febde", "scout"),
        ("enemy_brute", "#68aea2", "#c2d5d4", "brute"),
        ("enemy_flyer", "#67d5d3", "#dff7f4", "flyer"),
        ("enemy_shaman", "#63afa8", "#a2f48f", "shaman"),
        ("enemy_boss", "#77c5c0", "#d8f1ee", "boss"),
    ]
    for name, main, trim, kind in enemies:
        for frame in (0, 1):
            make_unit(SPRITES / f"{name}_{frame}.png", rgb(main), rgb(trim), kind, frame)


def make_wav(name, freqs, duration, decay=3.0, volume=0.35):
    sample_rate = 44100
    frames = int(sample_rate * duration)
    path = AUDIO / name
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        for i in range(frames):
            t = i / sample_rate
            env = math.exp(-decay * t / duration)
            val = 0.0
            for freq, amp in freqs:
                val += math.sin(2 * math.pi * freq * t) * amp
            val *= env * volume
            wf.writeframes(struct.pack("<h", max(-32767, min(32767, int(val * 32767)))))


def make_audio():
    make_wav("summon.wav", [(520, 0.6), (780, 0.35)], 0.22, 2.4, 0.36)
    make_wav("merge.wav", [(440, 0.35), (660, 0.55), (990, 0.28)], 0.34, 2.1, 0.38)
    make_wav("shoot.wav", [(880, 0.45), (1320, 0.18)], 0.08, 4.2, 0.18)
    make_wav("hit.wav", [(180, 0.45), (90, 0.25)], 0.11, 5.0, 0.26)
    make_wav("freeze.wav", [(300, 0.35), (450, 0.35), (620, 0.2)], 0.45, 1.4, 0.34)
    make_wav("cannon.wav", [(80, 0.65), (130, 0.3)], 0.55, 2.8, 0.45)
    make_wav("draft.wav", [(620, 0.3), (760, 0.35), (920, 0.35)], 0.32, 1.8, 0.33)
    make_wav("victory.wav", [(523, 0.35), (659, 0.35), (784, 0.35), (1047, 0.25)], 0.8, 1.1, 0.34)
    make_wav("defeat.wav", [(220, 0.38), (174, 0.32), (130, 0.25)], 0.7, 1.6, 0.32)


def main():
    ensure_dirs()
    make_scene()
    make_sprites()
    make_audio()
    print("Generated sprite and audio assets.")


if __name__ == "__main__":
    main()
