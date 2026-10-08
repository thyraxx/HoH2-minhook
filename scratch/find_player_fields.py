with open('res/assets.bin', 'rb') as f:
    data = f.read()

idx = data.find(b'scripts/Behaviors/Actors/Player/Player.as')
if idx != -1:
    chunk = data[idx:idx+10000]
    import re
    # Find all fields: Type m_name
    fields = re.findall(rb'([A-Za-z0-9_@<>]+)\s+(m_[A-Za-z0-9_]+)', chunk)
    for t, n in fields[:30]:
        print(f'{t.decode(errors="ignore")} {n.decode(errors="ignore")}')
