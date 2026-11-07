#!/usr/bin/env python3
"""Switch button glyphs for Mario Party 4's on-screen prompts.

Mario Party 4 draws its button prompts from the window icon atlas (WIN_ICON_ANM, game/window.c).
This draws Switch-style replacements for frames 0-12 (our own art, no Nintendo assets) and writes
them as tiled GX RGB5A3 into platforms/switch/switch_button_icons.h, which window.c copies over
the atlas when it loads.

The labels follow the port's controller mapping: the Switch buttons by label (A confirms, B
cancels), R for the GameCube's Z, and ZL/ZR for its analog L/R.

    python tools/switch/art/make_button_icons.py [--preview out.png]

Needs Pillow and Arial Black (Windows) or another bold font given with FONT=path.
"""
import os
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / "platforms" / "switch" / "switch_button_icons.h"
FONT = os.environ.get("FONT", "C:/Windows/Fonts/ariblk.ttf")
SS = 8  # supersampling factor

BODY = (44, 44, 48, 255)       # the Switch's dark buttons
RIM = (150, 150, 158, 255)     # thin light edge, so they read on dark backgrounds
LETTER = (255, 255, 255, 255)
STICK = (96, 96, 104, 255)
STICK_TOP = (58, 58, 64, 255)


def font(px):
    return ImageFont.truetype(FONT, px)


def finish(img, w, h):
    return img.resize((w, h), Image.LANCZOS)


def label(draw, text, cx, cy, px, fill=LETTER):
    f = font(px)
    box = draw.textbbox((0, 0), text, font=f)
    draw.text((cx - (box[0] + box[2]) / 2, cy - (box[1] + box[3]) / 2), text, font=f, fill=fill)


def round_button(w, h, text, scale=0.6):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = SS
    d.ellipse([m, m, W - m, H - m], fill=RIM)
    d.ellipse([m + 1.5 * SS, m + 1.5 * SS, W - m - 1.5 * SS, H - m - 1.5 * SS], fill=BODY)
    # a soft top sheen, as on a rounded physical button
    sheen = Image.new("L", (W, H), 0)
    ImageDraw.Draw(sheen).ellipse([W * 0.22, H * 0.12, W * 0.78, H * 0.48], fill=40)
    img.paste(Image.new("RGBA", (W, H), (255, 255, 255, 255)), (0, 0), sheen.filter(ImageFilter.GaussianBlur(SS)))
    label(d, text, W / 2, H / 2 + SS * 0.3, int(H * scale * (1.0 if len(text) == 1 else 0.7)))
    return finish(img, w, h)


def pill(w, h, text, height_frac=0.72):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ph = H * height_frac
    top = (H - ph) / 2
    r = ph / 2
    d.rounded_rectangle([SS, top, W - SS, top + ph], radius=r, fill=RIM)
    d.rounded_rectangle([SS + 1.5 * SS, top + 1.5 * SS, W - SS - 1.5 * SS, top + ph - 1.5 * SS],
                        radius=r - 1.5 * SS, fill=BODY)
    label(d, text, W / 2, H / 2 + SS * 0.3, int(ph * 0.58))
    return finish(img, w, h)


def stick(w, h, text):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([SS, SS, W - SS, H - SS], fill=RIM)
    d.ellipse([2.5 * SS, 2.5 * SS, W - 2.5 * SS, H - 2.5 * SS], fill=STICK)
    d.ellipse([W * 0.24, H * 0.24, W * 0.76, H * 0.76], fill=STICK_TOP)
    label(d, text, W / 2, H / 2 + SS * 0.3, int(H * 0.36))
    return finish(img, w, h)


def dpad(w, h):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    t = W * 0.36
    for inset, col in ((SS, RIM), (2.5 * SS, BODY)):
        d.rectangle([(W - t) / 2 + inset - SS, inset, (W + t) / 2 - inset + SS, H - inset], fill=col)
        d.rectangle([inset, (H - t) / 2 + inset - SS, W - inset, (H + t) / 2 - inset + SS], fill=col)
    return finish(img, w, h)


# Frame -> glyph. The atlas draws each GameCube trigger twice (a large and a small view).
FRAMES = {
    0: ("stick", "L"),
    1: ("dpad", None),
    2: ("round", "A"),
    3: ("round", "B"),
    4: ("round", "X"),
    5: ("round", "Y"),
    6: ("pill", "ZR"),
    7: ("pill", "ZR"),
    8: ("pill", "ZL"),
    9: ("pill", "ZL"),
    10: ("stick", "R"),
    11: ("pill", "R"),
    12: ("round", "+"),
}
SIZES = {11: (30, 24)}


def draw(kind, text, w, h):
    if kind == "round":
        return round_button(w, h, text, scale=0.7 if text == "+" else 0.6)
    if kind == "pill":
        return pill(w, h, text)
    if kind == "stick":
        return stick(w, h, text)
    return dpad(w, h)


def rgb5a3(r, g, b, a):
    if a >= 224:
        return 0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    return ((a >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)


def encode(img):
    """GX RGB5A3, 4x4 tiles over the size rounded up to 4, big-endian, transparent padding."""
    w, h = img.size
    px = img.load()
    out = bytearray()
    for by in range(0, (h + 3) & ~3, 4):
        for bx in range(0, (w + 3) & ~3, 4):
            for y in range(by, by + 4):
                for x in range(bx, bx + 4):
                    v = rgb5a3(*px[x, y]) if x < w and y < h else 0
                    out += v.to_bytes(2, "big")
    return bytes(out)


def main():
    images = {}
    for idx, (kind, text) in FRAMES.items():
        w, h = SIZES.get(idx, (24, 24))
        images[idx] = draw(kind, text, w, h)

    lines = [
        "// Generated by tools/switch/art/make_button_icons.py; do not edit.",
        "// Switch-style button glyphs (our own art) as tiled GX RGB5A3, copied over Mario Party 4's",
        "// window icon atlas (WIN_ICON_ANM) in game/window.c.",
        "#ifndef SWITCH_BUTTON_ICONS_H",
        "#define SWITCH_BUTTON_ICONS_H",
        "",
    ]
    for idx, img in images.items():
        data = encode(img)
        body = ", ".join(f"0x{b:02x}" for b in data)
        lines.append(f"static const unsigned char s_switch_icon_{idx:02d}[{len(data)}] = {{ {body} }};")
    lines += ["", "typedef struct { int idx, w, h; const unsigned char *data; } SwitchIconFrame;", "",
              "static const SwitchIconFrame switch_icon_frames[] = {"]
    for idx, img in images.items():
        lines.append(f"    {{ {idx}, {img.size[0]}, {img.size[1]}, s_switch_icon_{idx:02d} }},")
    lines += ["};", f"#define SWITCH_ICON_FRAME_COUNT {len(images)}", "", "#endif", ""]
    HEADER.write_text("\n".join(lines), encoding="ascii", newline="\n")
    print(f"wrote {HEADER} ({len(images)} frames)")

    if len(sys.argv) == 3 and sys.argv[1] == "--preview":
        scale = 4
        sheet = Image.new("RGBA", (len(images) * 32 * scale, 40 * scale), (90, 60, 140, 255))
        for n, img in enumerate(images.values()):
            big = img.resize((img.size[0] * scale, img.size[1] * scale), Image.NEAREST)
            sheet.alpha_composite(big, (n * 32 * scale + 4, 8 * scale))
        sheet.save(sys.argv[2])
        print(f"wrote {sys.argv[2]}")


if __name__ == "__main__":
    main()
