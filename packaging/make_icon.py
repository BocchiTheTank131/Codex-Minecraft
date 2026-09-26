"""Generate the Voxel Frontier Windows icon using only the Python standard library."""

from pathlib import Path
import struct
import sys
import zlib

BASE = 512
PIXELS = bytearray(BASE * BASE * 4)


def polygon(points, color):
    vertices = [(x * 2, y * 2) for x, y in points]
    left = max(0, min(x for x, _ in vertices))
    right = min(BASE, max(x for x, _ in vertices) + 1)
    top = max(0, min(y for _, y in vertices))
    bottom = min(BASE, max(y for _, y in vertices) + 1)
    for y in range(top, bottom):
        for x in range(left, right):
            inside = False
            previous = vertices[-1]
            for current in vertices:
                x1, y1 = previous
                x2, y2 = current
                if (y1 > y) != (y2 > y):
                    crossing = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
                    if x < crossing:
                        inside = not inside
                previous = current
            if inside:
                offset = (y * BASE + x) * 4
                PIXELS[offset:offset + 4] = bytes(color)


# A grass-topped voxel with a dark rim that reads clearly at 16 pixels.
polygon([(128, 18), (230, 70), (128, 124), (26, 70)], (17, 43, 54, 255))
polygon([(26, 70), (128, 124), (128, 235), (26, 181)], (17, 43, 54, 255))
polygon([(128, 124), (230, 70), (230, 181), (128, 235)], (17, 43, 54, 255))
polygon([(39, 84), (128, 131), (128, 218), (39, 174)], (151, 96, 55, 255))
polygon([(128, 131), (217, 84), (217, 174), (128, 218)], (108, 69, 47, 255))
polygon([(39, 84), (128, 131), (128, 151), (39, 105)], (53, 134, 74, 255))
polygon([(128, 131), (217, 84), (217, 105), (128, 151)], (37, 110, 69, 255))
polygon([(128, 31), (215, 73), (128, 116), (41, 73)], (86, 183, 83, 255))
polygon([(128, 31), (174, 54), (87, 98), (41, 73)], (111, 208, 102, 255))
polygon([(128, 31), (215, 73), (170, 95), (83, 52)], (75, 166, 77, 255))
polygon([(74, 68), (95, 57), (108, 63), (88, 74)], (141, 225, 119, 255))
polygon([(133, 84), (156, 72), (172, 80), (149, 92)], (125, 210, 104, 255))
polygon([(78, 105), (98, 116), (98, 127), (78, 116)], (86, 163, 82, 255))
polygon([(165, 113), (187, 102), (187, 116), (165, 128)], (60, 145, 77, 255))
polygon([(57, 123), (76, 134), (76, 148), (57, 137)], (184, 126, 71, 255))
polygon([(91, 164), (110, 174), (110, 187), (91, 177)], (109, 66, 45, 255))
polygon([(154, 164), (173, 154), (173, 169), (154, 179)], (147, 92, 54, 255))
polygon([(183, 131), (202, 121), (202, 136), (183, 146)], (82, 50, 40, 255))


def png(size):
    def sample(x, y):
        offset = (y * BASE + x) * 4
        return PIXELS[offset:offset + 4]

    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            # Four subpixel samples keep the smaller icons legible.
            channels = [0, 0, 0, 0]
            for dy in (0.25, 0.75):
                for dx in (0.25, 0.75):
                    px = min(BASE - 1, int((x + dx) * BASE / size))
                    py = min(BASE - 1, int((y + dy) * BASE / size))
                    for channel, value in enumerate(sample(px, py)):
                        channels[channel] += value
            raw.extend(round(value / 4) for value in channels)

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data +
                struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))

    header = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) +
            chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))


def main():
    target = Path(__file__).with_name('VoxelFrontier.ico')
    images = [(size, png(size)) for size in (16, 32, 48, 64, 128, 256)]
    offset = 6 + 16 * len(images)
    entries = bytearray()
    for size, content in images:
        entries.extend(struct.pack('<BBBBHHII', size % 256, size % 256,
                                   0, 0, 1, 32, len(content), offset))
        offset += len(content)
    target.write_bytes(struct.pack('<HHH', 0, 1, len(images)) + entries +
                       b''.join(content for _, content in images))
    if len(sys.argv) > 1:
        Path(sys.argv[1]).write_bytes(images[-1][1])
    print(target)


if __name__ == '__main__':
    main()
