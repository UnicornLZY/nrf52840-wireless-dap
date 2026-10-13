"""Verify the initial production source snapshot; no hardware or network access."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'source-manifest.json').read_text(encoding='utf-8'))
for entry in manifest['production_sources']:
    path = (root / entry['path']).resolve()
    if root not in path.parents:
        raise SystemExit('Manifest path outside repository')
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != entry['sha256']:
        raise SystemExit(f"Source changed: {entry['path']}")
print(f"PASS: {len(manifest['production_sources'])} production source files match the initial snapshot")
