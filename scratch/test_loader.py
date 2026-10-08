import glob
import os
import re

candidates = glob.glob(r"..\..\unpacked_assets_*") + glob.glob(r"..\unpacked_assets_*") + glob.glob(r"unpacked_assets_*")
assets_dir = candidates[0] if candidates else None
if not assets_dir:
    print("No unpacked_assets_* folder found, skipping scratch test.")
    exit(0)

with open(os.path.join(assets_dir, r"scripts\Behaviors\Actors\Player\PlayerRecord.as"), "r", encoding="utf-8") as f:
    source = f.read()

# Test finding RefreshModifiers
class_match = re.search(r"class\s+PlayerRecord\b[^{]*\{", source)
assert class_match is not None, "Class PlayerRecord not found"
search_pos = class_match.end()

func_match = re.search(r"(?:[A-Za-z0-9_<>@]+\s+)+RefreshModifiers\s*\(([^)]*)\)\s*(?:override|final)?\s*\{", source[search_pos:])
assert func_match is not None, "Method RefreshModifiers not found"

open_brace = search_pos + func_match.end() - 1

# Depth counting for closing brace
depth = 1
i = open_brace + 1
close_brace = -1
while i < len(source) and depth > 0:
    c = source[i]
    if c == '/' and i + 1 < len(source):
        if source[i+1] == '/':
            i = source.find('\n', i)
            if i == -1: break
            continue
        elif source[i+1] == '*':
            i = source.find('*/', i)
            if i == -1: break
            i += 2
            continue
    if c == '"':
        i += 1
        while i < len(source) and source[i] != '"':
            if source[i] == '\\': i += 1
            i += 1
        i += 1
        continue
    if c == '{': depth += 1
    elif c == '}': depth -= 1
    if depth == 0:
        close_brace = i
        break
    i += 1

print(f"Open brace at {open_brace}, close brace at {close_brace}")
print("Lines around close brace:")
print(source[close_brace - 60:close_brace + 20])
