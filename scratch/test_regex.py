import glob
import os
import re

candidates = glob.glob(r"..\..\unpacked_assets_*") + glob.glob(r"..\unpacked_assets_*") + glob.glob(r"unpacked_assets_*")
assets_dir = candidates[0] if candidates else None
if not assets_dir:
    print("No unpacked_assets_* folder found, skipping scratch test.")
    exit(0)

with open(os.path.join(assets_dir, 'scripts/Behaviors/Actors/Player/Player.as'), 'r', encoding='latin1') as f:
    source = f.read()

targetClass = 'Player'
targetMethod = 'Damage'
hookName = 'Player_Damage_Pre'

classRegex = re.compile(r'class\s+' + targetClass + r'\b[^{]*\{')
classMatch = classRegex.search(source)
print('classMatch:', classMatch is not None)
if classMatch:
    searchPos = classMatch.end()
    methodRegex = re.compile(r'(?:[A-Za-z0-9_<>@]+\s+)+' + targetMethod + r'\s*\(([^)]*)\)\s*(?:override|final)?\s*\{')
    searchSub = source[searchPos:]
    methodMatch = methodRegex.search(searchSub)
    print('methodMatch:', methodMatch is not None)
    if methodMatch:
        paramsStr = methodMatch.group(1)
        print('matched text:', methodMatch.group(0))
        print('paramsStr:', paramsStr)
        def is_value_type(t):
            vals = {'int', 'int8', 'int16', 'int32', 'int64', 'uint', 'uint8', 'uint16', 'uint32', 'uint64', 'float', 'double', 'bool', 'char', 'vec2', 'vec3', 'vec4', 'ivec2', 'ivec3', 'ivec4', 'quaternion', 'string', 'UnitPtr'}
            return t.strip().rstrip('&').strip() in vals

        args = ['@this']
        for p in paramsStr.split(','):
            p = p.strip()
            if not p: continue
            if '=' in p: p = p.split('=')[0].strip()
            tokens = p.split()
            pname = tokens[-1]
            ptype = tokens[-2] if len(tokens) > 1 else ''
            if pname.startswith('@'):
                args.append(pname)
            elif '@' in ptype:
                args.append('@' + pname)
            elif not is_value_type(ptype):
                args.append('@' + pname)
            else:
                args.append(pname)
        call = f'Hooks::Call("{hookName}", {", ".join(args)});'
        print('Generated call:', call)
