with open('res/assets.bin', 'rb') as f:
    data = f.read()

idx = 0
while True:
    idx = data.find(b'class WorldScript', idx)
    if idx == -1: break
    print('Found class WorldScript at 0x%X' % idx)
    chunk = data[idx:idx+300]
    clean = ''.join(chr(b) if 32 <= b < 127 or b in (10, 13, 9) else '.' for b in chunk)
    print(clean)
    idx += 1
