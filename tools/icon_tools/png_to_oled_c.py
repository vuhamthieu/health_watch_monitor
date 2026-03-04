#!/usr/bin/env python3
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit(
        "Pillow is required. Install with: pip install pillow"
    ) from exc


def sanitize_symbol(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch.upper())
        else:
            out.append('_')
    symbol = ''.join(out).strip('_')
    while '__' in symbol:
        symbol = symbol.replace('__', '_')
    return symbol or 'ICON'


def process_image(img_path: Path, width: int, height: int, threshold: int, invert: bool) -> tuple[list[int], str]:
    img = Image.open(img_path).convert("RGBA")
    
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    
    bg = Image.alpha_composite(bg, img)
    
    gray = bg.convert("L")
    
    if gray.size != (width, height):
        gray = gray.resize((width, height), Image.Resampling.NEAREST)

    data = []
    ascii_preview = ""
    
    for y in range(height):
        row_bytes = (width + 7) // 8
        for b in range(row_bytes):
            value = 0
            for bit in range(8):
                x = b * 8 + bit
                if x >= width:
                    continue
                px = gray.getpixel((x, y))
                
                on = px < threshold
                if invert:
                    on = not on
                
                if on:
                    value |= (1 << (7 - bit))
                    ascii_preview += "██" 
                else:
                    ascii_preview += ".."
            data.append(value)
        ascii_preview += "\n"
        
    return data, ascii_preview


def format_c_array(values: list[int], bytes_per_row: int) -> str:
    lines = []
    for i in range(0, len(values), bytes_per_row):
        row = values[i:i + bytes_per_row]
        line = ', '.join(f'0x{v:02X}' for v in row)
        lines.append(f'    {line},')
    return '\n'.join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description='Convert PNG icon to SH1106-style C array (row-major, MSB-first).')
    parser.add_argument('input_png', type=Path)
    parser.add_argument('--width', type=int, default=16)
    parser.add_argument('--height', type=int, default=16)
    parser.add_argument('--name', type=str, default=None, help='C symbol name, e.g. MICON_HEART')
    parser.add_argument('--threshold', type=int, default=128)
    parser.add_argument('--invert', action='store_true', help='Invert black/white during conversion')
    parser.add_argument('--out-c', type=Path, required=True)
    parser.add_argument('--out-h', type=Path, required=True)

    args = parser.parse_args()

    symbol = sanitize_symbol(args.name or args.input_png.stem)
    
    values, preview = process_image(args.input_png, args.width, args.height, args.threshold, args.invert)
    
    bytes_per_row = (args.width + 7) // 8

    include_guard = sanitize_symbol(args.out_h.stem) + '_H'

    args.out_h.parent.mkdir(parents=True, exist_ok=True)
    args.out_c.parent.mkdir(parents=True, exist_ok=True)

    h_content = f'''#ifndef {include_guard}\n#define {include_guard}\n\n#include <stdint.h>\n\n#define {symbol}_WIDTH {args.width}u\n#define {symbol}_HEIGHT {args.height}u\nextern const uint8_t {symbol}[{len(values)}];\n\n#endif\n'''

    c_content = f'''#include "{args.out_h.name}"\n\n/* row-major, MSB-first, {bytes_per_row} bytes per row */\nconst uint8_t {symbol}[{len(values)}] = {{\n{format_c_array(values, bytes_per_row)}\n}};\n'''

    args.out_h.write_text(h_content)
    args.out_c.write_text(c_content)

    print(f'Generated: {args.out_h}')
    print(f'Generated: {args.out_c}')
    print("\n--- IMAGE WILL DISPLAY ON OLED: ---")
    print(preview)


if __name__ == '__main__':
    main()