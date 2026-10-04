"""Build matching desktop icons from the SendToCmd paper-plane geometry.

Uses only the Python standard library so the Windows cross-build can regenerate
the icons without an image editor or an extra Python package.
"""

from pathlib import Path
import struct
import zlib


ROOT = Path(__file__).resolve().parent.parent
RESOURCES = ROOT / "resources"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
VIEW_START = -24.0
VIEW_SIZE = 560.0
OUTLINE = ((0, 145), (512, 0), (367, 512), (242, 387), (87, 425), (125, 270))
UPPER = ((0, 145), (512, 0), (125, 270))
FOLD = ((125, 270), (512, 0), (242, 387), (87, 425))
COLORS = ((55, 150, 108), (79, 173, 126), (38, 121, 82))


def inside(x, y, polygon):
    result = False
    previous = polygon[-1]
    for current in polygon:
        x1, y1 = previous
        x2, y2 = current
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            result = not result
        previous = current
    return result


def sample(x, y):
    if inside(x, y, FOLD):
        return COLORS[2]
    if inside(x, y, UPPER):
        return COLORS[1]
    if inside(x, y, OUTLINE):
        return COLORS[0]
    return None


def render(size):
    count = 4 if size <= 64 else 2
    pixels = []
    for row in range(size):
        output = []
        for column in range(size):
            sums = [0, 0, 0]
            covered = 0
            for subrow in range(count):
                y = VIEW_START + VIEW_SIZE * (row + (subrow + 0.5) / count) / size
                for subcolumn in range(count):
                    x = VIEW_START + VIEW_SIZE * (column + (subcolumn + 0.5) / count) / size
                    color = sample(x, y)
                    if color is None:
                        continue
                    covered += 1
                    for channel in range(3):
                        sums[channel] += color[channel]
            output.append(tuple(round(value / covered) for value in sums) +
                          (round(255 * covered / (count * count)),) if covered else (0, 0, 0, 0))
        pixels.append(output)
    return pixels


def png_chunk(name, data):
    return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data))


def png_image(size, pixels):
    rows = bytearray()
    for row in pixels:
        rows.append(0)
        for rgba in row:
            rows.extend(rgba)
    return (b"\x89PNG\r\n\x1a\n" +
            png_chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            png_chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + png_chunk(b"IEND", b""))


def dib_image(size, pixels):
    data = bytearray(struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0,
                                 size * size * 4, 0, 0, 0, 0))
    for row in reversed(pixels):
        for red, green, blue, alpha in row:
            data.extend((blue, green, red, alpha))
    mask_stride = ((size + 31) // 32) * 4
    for row in reversed(pixels):
        mask = bytearray(mask_stride)
        for column, (_, _, _, alpha) in enumerate(row):
            if alpha == 0:
                mask[column // 8] |= 1 << (7 - column % 8)
        data.extend(mask)
    return bytes(data)


def write_ico(pixels_by_size, png_by_size):
    sizes = SIZES[:-1]
    images = [png_by_size[size] if size == 256 else dib_image(size, pixels_by_size[size]) for size in sizes]
    directory = bytearray(struct.pack("<HHH", 0, 1, len(sizes)))
    offset = 6 + 16 * len(sizes)
    for size, data in zip(sizes, images):
        directory.extend(struct.pack("<BBBBHHII", size if size < 256 else 0,
                                     size if size < 256 else 0, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    (RESOURCES / "SendToCmd.ico").write_bytes(bytes(directory) + b"".join(images))


def write_icns(png_by_size):
    records = []
    for size, name in ((16, b"icp4"), (32, b"icp5"), (64, b"icp6"),
                       (128, b"ic07"), (256, b"ic08"), (512, b"ic09")):
        data = png_by_size[size]
        records.append(name + struct.pack(">I", len(data) + 8) + data)
    contents = b"".join(records)
    (RESOURCES / "SendToCmd.icns").write_bytes(b"icns" + struct.pack(">I", len(contents) + 8) + contents)


def write_svg():
    polygons = ((OUTLINE, "#37966c"), (UPPER, "#4fad7e"), (FOLD, "#267952"))
    shapes = "\n".join("  <polygon points=\"%s\" fill=\"%s\"/>" %
                       (" ".join(f"{x},{y}" for x, y in points), color) for points, color in polygons)
    (RESOURCES / "sendtocmd.svg").write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="-24 -24 560 560">\n' +
        shapes + "\n</svg>\n", encoding="utf-8")


def main():
    icon_dir = RESOURCES / "icons"
    icon_dir.mkdir(parents=True, exist_ok=True)
    pixels_by_size = {size: render(size) for size in SIZES}
    png_by_size = {size: png_image(size, pixels) for size, pixels in pixels_by_size.items()}
    for size in SIZES[:-1]:
        (icon_dir / f"sendtocmd-{size}.png").write_bytes(png_by_size[size])
    write_svg()
    write_ico(pixels_by_size, png_by_size)
    write_icns(png_by_size)
    print("Generated SendToCmd plane SVG, PNG, ICO and ICNS icons")


if __name__ == "__main__":
    main()
