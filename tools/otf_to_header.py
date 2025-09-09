#!/usr/bin/env python3
import sys
import os

def sanitize_symbol(name: str) -> str:
    # Lowercase, replace non-alnum with underscore
    out = []
    for ch in name.lower():
        if ch.isalnum():
            out.append(ch)
        else:
            out.append('_')
    # collapse multiple underscores
    s = ''.join(out)
    while '__' in s:
        s = s.replace('__', '_')
    return s.strip('_')

def main():
    if len(sys.argv) < 3:
        print("Usage: otf_to_header.py <input.otf> <output.h> [symbol_base]", file=sys.stderr)
        sys.exit(1)
    inp = sys.argv[1]
    outp = sys.argv[2]
    base = sys.argv[3] if len(sys.argv) > 2 else None
    if not base:
        base = os.path.splitext(os.path.basename(inp))[0]
    sym_base = sanitize_symbol(base) + '_otf'
    sym_data = sym_base
    sym_size = sym_base + '_len'
    guard = sanitize_symbol(os.path.basename(outp)).upper() + '_INCLUDED'

    with open(inp, 'rb') as f:
        data = f.read()
    # write header
    os.makedirs(os.path.dirname(outp), exist_ok=True)
    with open(outp, 'w', newline='\n') as h:
        h.write(f"#ifndef {guard}\n#define {guard}\n\n")
        h.write("#include <cstddef>\n#include <cstdint>\n\n")
        h.write(f"static const unsigned char {sym_data}[] = {{\n")
        # format bytes as 12 per line
        for i in range(0, len(data), 12):
            chunk = data[i:i+12]
            line = ', '.join(f'0x{b:02X}' for b in chunk)
            if i + 12 < len(data):
                h.write(f"    {line},\n")
            else:
                h.write(f"    {line}\n")
        h.write("};\n")
        h.write(f"static const size_t {sym_size} = sizeof({sym_data});\n\n")
        h.write(f"#endif // {guard}\n")

if __name__ == '__main__':
    main()


