"""Generate original sample assets; Python standard library only. No font files."""
from pathlib import Path
import math
import struct
import wave


def main() -> None:
    root = Path(__file__).resolve().parents[1] / "Assets"
    root.mkdir(exist_ok=True)
    width = height = 64
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            border = x < 3 or y < 3 or x >= width - 3 or y >= height - 3
            light = ((x // 8) + (y // 8)) % 2 == 0
            r, g, b = (255, 255, 255) if border else ((90, 180, 230) if light else (45, 110, 175))
            pixels.extend((b, g, r))
    offset = 54
    header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    (root / "player.bmp").write_bytes(header + dib + pixels)
    rate = 22050
    frames = int(rate * 0.18)
    audio = bytearray()
    for i in range(frames):
        t = i / rate
        envelope = min(1.0, i / 160.0) * max(0.0, 1.0 - i / frames)
        sample = int(0.3 * 32767 * envelope * math.sin(2.0 * math.pi * 660.0 * t))
        audio.extend(struct.pack("<h", sample))
    with wave.open(str(root / "confirm.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(audio)
    print(f"Generated sample assets: {root}")


if __name__ == "__main__":
    main()
