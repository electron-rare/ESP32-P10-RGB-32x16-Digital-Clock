#!/usr/bin/env python3

import sys
import re

def convert_bdf_to_adafruit(bdf_filename, start_char=32, end_char=255):
    """Convert BDF font to Adafruit GFX format"""
    
    with open(bdf_filename, 'r') as f:
        lines = f.readlines()
    
    # Parse BDF header
    font_name = "Font"
    font_size = 9
    
    for line in lines:
        if line.startswith('FONT '):
            # Extract font name
            parts = line.split('-')
            if len(parts) > 2:
                font_name = parts[2].replace(' ', '')
        elif line.startswith('PIXEL_SIZE '):
            font_size = int(line.split()[1])
    
    # Clean font name
    font_name = re.sub(r'[^a-zA-Z0-9]', '', font_name)
    header_name = f"{font_name}{font_size}ptLat1"
    
    print(f"const uint8_t {header_name}Bitmaps[] PROGMEM = {{")
    
    # Parse characters
    char_data = {}
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith('STARTCHAR'):
            # Parse character
            char_code = None
            width = 0
            height = 0
            x_offset = 0
            y_offset = 0
            advance = 0
            bitmap_data = []
            
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('ENDCHAR'):
                line = lines[i].strip()
                if line.startswith('ENCODING '):
                    char_code = int(line.split()[1])
                elif line.startswith('DWIDTH '):
                    advance = int(line.split()[1])
                elif line.startswith('BBX '):
                    parts = line.split()
                    width = int(parts[1])
                    height = int(parts[2])
                    x_offset = int(parts[3])
                    y_offset = int(parts[4])
                elif line.startswith('BITMAP'):
                    i += 1
                    while i < len(lines) and not lines[i].strip().startswith('ENDCHAR'):
                        hex_line = lines[i].strip()
                        if hex_line:
                            bitmap_data.append(hex_line)
                        i += 1
                    break
                i += 1
            
            if char_code is not None and start_char <= char_code <= end_char:
                char_data[char_code] = {
                    'width': width,
                    'height': height,
                    'advance': advance,
                    'x_offset': x_offset,
                    'y_offset': y_offset,
                    'bitmap': bitmap_data
                }
        i += 1
    
    # Generate bitmap data
    bitmap_bytes = []
    for char_code in sorted(char_data.keys()):
        char = char_data[char_code]
        for hex_line in char['bitmap']:
            # Convert hex string to bytes
            for i in range(0, len(hex_line), 2):
                if i + 1 < len(hex_line):
                    byte_val = int(hex_line[i:i+2], 16)
                    bitmap_bytes.append(f"0x{byte_val:02X}")
    
    # Print bitmap data
    for i, byte_val in enumerate(bitmap_bytes):
        if i % 12 == 0:
            print("\n  ", end="")
        print(f"{byte_val}", end="")
        if i < len(bitmap_bytes) - 1:
            print(", ", end="")
    
    print("\n};")
    print()
    
    # Generate glyph data
    print(f"const GFXglyph {header_name}Glyphs[] PROGMEM = {{")
    offset = 0
    for char_code in sorted(char_data.keys()):
        char = char_data[char_code]
        bitmap_size = len(char['bitmap']) * ((char['width'] + 7) // 8)
        print(f"  {{ {offset}, {char['width']}, {char['height']}, {char['advance']}, {char['x_offset']}, {char['y_offset']} }}", end="")
        if char_code < max(char_data.keys()):
            print(",")
        else:
            print("")
        offset += bitmap_size
    
    print("};")
    print()
    
    # Generate font structure
    first_char = min(char_data.keys())
    last_char = max(char_data.keys())
    print(f"const GFXfont {header_name} PROGMEM = {{")
    print(f"  (uint8_t  *){header_name}Bitmaps,")
    print(f"  (GFXglyph *){header_name}Glyphs,")
    print(f"  {first_char}, {last_char}, {font_size} }};")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 bdf2adafruit.py <font.bdf>")
        sys.exit(1)
    
    convert_bdf_to_adafruit(sys.argv[1])
