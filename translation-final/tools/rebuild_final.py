"""Rebuild and validate the supplied Xbox 360 LH2 language resource."""
from pathlib import Path
import json, re, hashlib
from lh2_text import parse, rebuild

root = Path(__file__).resolve().parents[1]
td = root / 'translation'
rows = json.loads((td / 'zh_CN_final.json').read_text(encoding='utf-8-sig'))
original = (td / 'english_original.lh2').read_bytes()
source = parse(original)
assert len(rows) == len(source)
assert len({r['id'] for r in rows}) == len(rows)
pattern = r'%(?:\d+\$)?[-+0#]*\d*(?:\.\d+)?[a-zA-Z]|%\d+|\{[^}]+\}'
for old, row in zip(source, rows):
    assert all(row[k] == old[k] for k in ('index','id','source')), row['id']
    assert isinstance(row['translation'], str) and row['translation'], row['id']
    for token_pattern in (r'\\+n', r'<[^>]+>', pattern):
        assert sorted(re.findall(token_pattern, row['source'])) == sorted(re.findall(token_pattern, row['translation'])), row['id']
    assert re.findall(r'\\+n', row['source']) == re.findall(r'\\+n', row['translation']), row['id']
    assert row['source'].count('\n') == row['translation'].count('\n'), row['id']
assert rebuild(original, source) == original
output = rebuild(original, rows)
decoded = parse(output)
assert all(a['id'] == b['id'] and a['translation'] == b['source'] for a,b in zip(rows,decoded))
assert rebuild(output, decoded) == output
(td / 'zh_CN_final.lh2').write_bytes(output)
report = {
    'total_entries': len(rows),
    'translated_entries': sum(r['translation'] != r['source'] for r in rows),
    'preserved_entries': sum(r['translation'] == r['source'] for r in rows),
    'pending_entries': sum(not r['translation'] for r in rows),
    'scope': 'All entries in the supplied english_original.lh2 only',
    'source_index_id_integrity': True,
    'control_tokens_and_markup_preserved': True,
    'source_binary_roundtrip': True,
    'translated_binary_roundtrip': True,
    'original_sha256': hashlib.sha256(original).hexdigest(),
    'output_sha256': hashlib.sha256(output).hexdigest(),
    'output_bytes': len(output),
    'translation_complete_for_supplied_entries': True,
    'full_game_text_extraction_verified': False,
    'full_viv_integrated': False,
    'chinese_font_hardware_validated': False,
    'gameplay_and_layout_verified': False,
    'installable_patch_ready': False
}
(root / 'validation_report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2),encoding='utf8')
print(json.dumps(report,ensure_ascii=False,indent=2))
