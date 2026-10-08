with open('res/assets.bin', 'rb') as f:
    data = f.read()

idx = data.find(b'scripts/Behaviors/Actors/Player/PlayerBase.as')
print('PlayerBase.as offset:', hex(idx))
if idx != -1:
    chunk = data[idx:idx+4000]
    clean = ''.join(chr(b) if 32 <= b < 127 or b in (10, 13, 9) else '.' for b in chunk)
    print(clean)
