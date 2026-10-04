"""Generate a multi-size, alpha-smoothed Windows icon without dependencies."""

from pathlib import Path
import math
import struct
import zlib


SIZES = (16, 24, 32, 48, 64, 128, 256)
BACKGROUND = (16, 125, 132)
PROMPT = (255, 255, 255)
CURSOR = (171, 239, 196)


def segment_distance(x, y, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    fraction = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - (x1 + fraction * dx), y - (y1 + fraction * dy))


def sample(x, y):
    if math.hypot(x - 0.5, y - 0.5) > 0.48:
        return None
    if segment_distance(x, y, 0.29, 0.28, 0.51, 0.5) <= 0.045 or segment_distance(x, y, 0.51, 0.5, 0.29, 0.72) <= 0.045:
        return PROMPT
    if segment_distance(x, y, 0.58, 0.72, 0.76, 0.72) <= 0.043:
        return CURSOR
    return BACKGROUND


def render_pixels(size):
    samples = 4
    pixels = []
    for y in range(size):
        row = []
        for x in range(size):
            colors = [0, 0, 0]
            covered = 0
            for sy in range(samples):
                for sx in range(samples):
                    color = sample((x + (sx + 0.5) / samples) / size,
                                   (y + (sy + 0.5) / samples) / size)
                    if color is not None:
                        covered += 1
                        for channel in range(3):
                            colors[channel] += color[channel]
            if covered:
                rgba = tuple(round(value / covered) for value in colors) + (round(255 * covered / (samples * samples)),)
            else:
                rgba = (0, 0, 0, 0)
            row.append(rgba)
        pixels.append(row)

    return pixels


def icon_image(size, pixels):
    image = bytearray(struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0,
                                  size * size * 4, 0, 0, 0, 0))
    for row in reversed(pixels):
        for red, green, blue, alpha in row:
            image.extend((blue, green, red, alpha))
    mask_stride = ((size + 31) // 32) * 4
    for row in reversed(pixels):
        mask = bytearray(mask_stride)
        for x, (_, _, _, alpha) in enumerate(row):
            if alpha == 0:
                mask[x // 8] |= 1 << (7 - x % 8)
        image.extend(mask)
    return bytes(image)


def png_chunk(name, contents):
    return struct.pack('>I', len(contents)) + name + contents + struct.pack('>I', zlib.crc32(name + contents))


def png_image(size, pixels):
    rows = bytearray()
    for row in pixels:
        rows.append(0)
        for rgba in row:
            rows.extend(rgba)
    return (b'\x89PNG\r\n\x1a\n'
            + png_chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + png_chunk(b'IDAT', zlib.compress(bytes(rows), 9))
            + png_chunk(b'IEND', b''))


def main():
    images = [png_image(size, render_pixels(size)) if size == 256 else icon_image(size, render_pixels(size))
              for size in SIZES]
    directory = bytearray(struct.pack('<HHH', 0, 1, len(SIZES)))
    offset = 6 + 16 * len(SIZES)
    for size, image in zip(SIZES, images):
        directory.extend(struct.pack('<BBBBHHII', size if size < 256 else 0,
                                     size if size < 256 else 0, 0, 0, 1, 32,
                                     len(image), offset))
        offset += len(image)
    destination = Path(__file__).resolve().parent.parent / 'SendToCmd.ico'
    destination.write_bytes(bytes(directory) + b''.join(images))
    print(destination)


if __name__ == '__main__':
    main()
