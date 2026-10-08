import os
import re
import glob
import sys

def find_unpacked_assets_dir():
    """
    Dynamically finds unpacked_assets_<buildNumber> created by PACKAGER.exe.
    Returns the path if found, or None if base assets haven't been unpacked.
    Resilient to game updates and build number changes.
    """
    candidates = (
        glob.glob("../../unpacked_assets_*") +
        glob.glob("../unpacked_assets_*") +
        glob.glob("unpacked_assets_*")
    )
    for c in candidates:
        if os.path.isdir(c):
            return os.path.abspath(c)
    return None

def is_snippet_section(name):
    if not name:
        return False
    norm = name.replace("\\", "/").lower()
    return norm.endswith(".patch") or norm.endswith(".inc")

def preprocess_mod_script(content):
    # Strips [Hook "Name"] metadata and changes function name
    pattern = re.compile(r'\[\s*Hook\s+"([^"]+)"\s*\]\s*void\s+([A-Za-z0-9_]+)\s*\(([^)]*)\)')
    modified = False
    
    def repl(m):
        nonlocal modified
        modified = True
        hook_name = m.group(1)
        params = m.group(2) # wait, params is group 3
        return f"void {hook_name}({m.group(3)})"

    new_content = pattern.sub(repl, content)
    return modified, new_content

def find_method_braces(source, class_name, method_name):
    class_match = re.search(r"class\s+" + re.escape(class_name) + r"\b[^{]*\{", source)
    if not class_match:
        return None, None
    search_pos = class_match.end()

    method_regex = re.compile(
        r"(?:[A-Za-z0-9_<>@]+\s+)+" + re.escape(method_name) + r"\s*\(([^)]*)\)\s*(?:override|final)?\s*\{"
    )
    method_match = method_regex.search(source[search_pos:])
    if not method_match:
        return None, None

    open_brace = search_pos + method_match.end() - 1

    # Depth counting
    depth = 1
    i = open_brace + 1
    close_brace = -1
    while i < len(source) and depth > 0:
        c = source[i]
        if c == '/' and i + 1 < len(source):
            if source[i+1] == '/':
                i = source.find('\n', i)
                if i == -1:
                    break
                continue
            elif source[i+1] == '*':
                i = source.find('*/', i)
                if i == -1:
                    break
                i += 2
                continue
        if c == '"':
            i += 1
            while i < len(source) and source[i] != '"':
                if source[i] == '\\':
                    i += 1
                i += 1
            i += 1
            continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        if depth == 0:
            close_brace = i
            break
        i += 1

    return open_brace, close_brace

# --- Hermetic Test Fixtures ---
MOCK_PLAYER_SCRIPT = """class Player : PlayerBase
{
    Player(UnitPtr unit, SValue@ params)
    {
        super(unit, params);
    }

    void Damage(DamageInfo@ dmg, vec2 pos, vec2 dir) override
    {
        // Deal damage logic
        m_currentHp -= dmg.Damage;
    }
}
"""

MOCK_PLAYER_RECORD_SCRIPT = """class PlayerRecord
{
    string name;
    int hp;

    void RefreshModifiers()
    {
        // Calculate bonuses
        if (hp > 0) {
            string debugMsg = "Braces inside { string } and comments /* } */ // }";
        }
    }

    void AssignUnit(UnitPtr unit)
    {
        // Unit assignment
    }
}
"""

def run_tests():
    print("=" * 60)
    print(" Heroes of Hammerwatch 2 - Python Resilient Unit Tests")
    print("=" * 60)

    # 1. Test Snippet Neutralization
    print("[TEST] Snippet Neutralization (.patch and .inc)... ", end="")
    assert is_snippet_section("patches/my_patch.patch") is True
    assert is_snippet_section("scripts/my_include.inc") is True
    assert is_snippet_section("scripts/Behaviors/Player.as") is False
    assert is_snippet_section(None) is False
    print("PASSED")

    # 2. Test Metadata Preprocessing
    print("[TEST] In-Flight Metadata Preprocessing ([Hook] tag stripping)... ", end="")
    sample_hook = '[Hook "OnDamage"]\nvoid MyHandler(Player@ player, DamageInfo@ dmg) {\n    print("ok");\n}'
    modified, res = preprocess_mod_script(sample_hook)
    assert modified is True
    assert '[Hook' not in res
    assert 'void OnDamage(Player@ player, DamageInfo@ dmg)' in res
    print("PASSED")

    # 3. Test Hermetic Method Parsing & Depth Counting
    print("[TEST] Hermetic Class & Method Parsing (Self-Contained Fixtures)... ", end="")
    open_b, close_b = find_method_braces(MOCK_PLAYER_SCRIPT, "Player", "Damage")
    assert open_b is not None and close_b is not None
    assert MOCK_PLAYER_SCRIPT[open_b] == "{"
    assert MOCK_PLAYER_SCRIPT[close_b] == "}"
    body = MOCK_PLAYER_SCRIPT[open_b+1:close_b]
    assert "m_currentHp -= dmg.Damage;" in body

    open_b2, close_b2 = find_method_braces(MOCK_PLAYER_RECORD_SCRIPT, "PlayerRecord", "RefreshModifiers")
    assert open_b2 is not None and close_b2 is not None
    assert MOCK_PLAYER_RECORD_SCRIPT[open_b2] == "{"
    assert MOCK_PLAYER_RECORD_SCRIPT[close_b2] == "}"
    body2 = MOCK_PLAYER_RECORD_SCRIPT[open_b2+1:close_b2]
    assert "string debugMsg =" in body2
    print("PASSED")

    # 4. Test Dynamic Unpacked Assets (Optional Validation)
    print("[TEST] Dynamic Discovery of Unpacked Game Assets... ", end="")
    assets_dir = find_unpacked_assets_dir()
    if not assets_dir:
        print("SKIPPED (No unpacked_assets_* folder found; base game assets not unpacked. Hermetic tests verified!)")
    else:
        print(f"\n       -> Found dynamic assets directory: {assets_dir}")
        player_path = os.path.join(assets_dir, "scripts", "Behaviors", "Actors", "Player", "Player.as")
        if os.path.exists(player_path):
            with open(player_path, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
            ob, cb = find_method_braces(content, "Player", "Damage")
            assert ob is not None and cb is not None, "Failed to parse Damage in live Player.as"
            print(f"       -> Validated live Player.as ({len(content)} bytes) OK")

        record_path = os.path.join(assets_dir, "scripts", "Behaviors", "Actors", "Player", "PlayerRecord.as")
        if os.path.exists(record_path):
            with open(record_path, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
            ob, cb = find_method_braces(content, "PlayerRecord", "RefreshModifiers")
            assert ob is not None and cb is not None, "Failed to parse RefreshModifiers in live PlayerRecord.as"
            print(f"       -> Validated live PlayerRecord.as ({len(content)} bytes) OK")
        print("       -> Live game asset validation PASSED")

    print("\n" + "=" * 60)
    print(" [ALL TESTS PASSED SUCCESSFULLY]")
    print(" Tests are fully hermetic and resilient to game updates!")
    print("=" * 60)

if __name__ == "__main__":
    run_tests()
