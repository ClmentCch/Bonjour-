#!/usr/bin/env python3
"""Generate assets/icon.jpg, the 256x256 home menu icon.

Black canvas, "Bonjour !" in a serif face with the same horizontal rainbow
gradient the application animates. Requires Pillow.

Usage: python3 tools/make_icon.py [font.ttf]
"""
import colorsys
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

SIZE = 256
TEXT = "Bonjour !"
HERE = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_FONT = HERE / "assets" / "fonts" / "LiberationSerif-Regular.ttf"
OUTPUT = HERE / "assets" / "icon.jpg"


def gradient_mask(width, height):
    """Horizontal rainbow mask, matching the renderer's hue sweep."""
    mask = Image.new("RGB", (width, height))
    pixels = mask.load()
    for x in range(width):
        hue = x / max(1, width)
        r, g, b = colorsys.hsv_to_rgb(hue, 1.0, 1.0)
        colour = (int(r * 255), int(g * 255), int(b * 255))
        for y in range(height):
            pixels[x, y] = colour
    return mask


def main():
    font_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_FONT
    if not font_path.exists():
        sys.exit(f"font not found: {font_path}")

    # Find the largest size that keeps the text inside a comfortable margin.
    text_layer = Image.new("L", (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(text_layer)
    size = 96
    while size > 12:
        font = ImageFont.truetype(str(font_path), size)
        left, top, right, bottom = draw.textbbox((0, 0), TEXT, font=font)
        if right - left <= SIZE - 32 and bottom - top <= SIZE - 64:
            break
        size -= 2

    width = right - left
    height = bottom - top
    draw.text(((SIZE - width) / 2 - left, (SIZE - height) / 2 - top), TEXT, font=font, fill=255)

    bbox = text_layer.getbbox()
    colours = gradient_mask(SIZE, SIZE)

    icon = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    icon.paste(colours, (0, 0), text_layer)
    icon.save(OUTPUT, "JPEG", quality=94, subsampling=0)

    print(f"wrote {OUTPUT} ({SIZE}x{SIZE}, {size}px, text box {bbox})")


if __name__ == "__main__":
    main()
