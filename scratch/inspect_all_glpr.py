with open('res/assets.bin', 'rb') as f:
    data = f.read()

idx = 0
found = []
while True:
    idx = data.find(b'GetLocalPlayerRecord', idx)
    if idx == -1: break
    found.append(idx)
    idx += 1

print(f'Total occurrences: {len(found)}')
for i, off in enumerate(found):
    print(f'=== Occurrence {i+1} at 0x{off:X} ===')
    chunk = data[max(0, off-200):min(len(data), off+300)]
    clean = ''.join(chr(b) if 32 <= b < 127 or b in (10, 13, 9) else '.' for b in chunk)
    print(clean)
