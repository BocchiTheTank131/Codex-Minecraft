"""Regenerate original native-16px weather art without changing legacy texels.

Run after the existing world/inventory atlas generators. No runtime dependency.
"""
from pathlib import Path
import struct

ASSETS = Path(__file__).resolve().parents[1] / "assets"


def fire_tile():
    pixels = bytearray()
    for y in range(16):
        for x in range(16):
            height = 5 + (x * 7 + 3) % 9
            visible = y >= 16 - height and 2 <= x <= 13
            core = visible and y >= 10 and 5 <= x <= 10
            rgb = (255, 225, 89) if core else (244, 148 if y > 8 else 96, 35)
            pixels.extend((*rgb, 255) if visible else (0, 0, 0, 0))
    return pixels


def main():
    path = ASSETS / "crafting_blocks.rgba"
    data = path.read_bytes()
    width, height = struct.unpack_from("<ii", data)
    if width not in (224, 240) or height != 16:
        raise ValueError("Unexpected crafting atlas layout")
    tile = fire_tile()
    pixels = bytearray()
    for y in range(16):
        pixels.extend(data[8+y*width*4:8+(y*width+224)*4])
        pixels.extend(tile[y*64:(y+1)*64])
    path.write_bytes(struct.pack("<ii", 240, 16) + pixels)

    path = ASSETS / "item_icons_expansion.rgba"
    data = bytearray(path.read_bytes())
    width, height = struct.unpack_from("<ii", data)
    if (width, height) != (640, 704):
        raise ValueError("Unexpected inventory atlas layout")
    rod = [[(0, 0, 0, 0) for _ in range(16)] for _ in range(16)]
    for y in range(2, 14):
        for x in range(7, 10):
            rod[y][x] = ((234, 156, 95, 255), (171, 88, 48, 255), (104, 51, 31, 255))[x-7]
    for y in range(1, 4):
        for x in range(6, 11):
            rod[y][x] = (255, 190, 123, 255) if y == 1 else (188, 102, 58, 255)
    for y in range(13, 15):
        for x in range(5, 12):
            rod[y][x] = (145, 75, 42, 255)
    col, row = 102 % 10, 102 // 10
    for y in range(64):
        for x in range(64):
            offset = 8 + ((row*64+y)*width+col*64+x)*4
            data[offset:offset+4] = bytes(rod[y//4][x//4])
    path.write_bytes(data)


if __name__ == "__main__":
    main()
