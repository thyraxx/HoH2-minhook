import re

content = open('mods/CustomHooksMod/scripts/hookstest.as', 'r').read()

hookAttrRegex = re.compile(r'\[\s*Hook\s+"([A-Za-z0-9_]+)"\s*\]\s*([A-Za-z0-9_<>@]+\s+)([A-Za-z0-9_]+)')
processed = hookAttrRegex.sub(r'\2\1', content)

stripHookRegex = re.compile(r'\[\s*Hook(?:\s+"[^"]+")?\s*\]\s*')
processed = stripHookRegex.sub('', processed)

print("Original:")
print(content)
print("Processed:")
print(processed)
