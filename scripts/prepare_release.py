"""Create a source-only candidate from explicit allowlists; never package the workspace."""
from pathlib import Path
import hashlib
import shutil

root = Path(__file__).resolve().parents[1]
destination = root / 'release/grundfos-magna3-0.1.0'
component_files = [
    '__init__.py', 'grundfos_magna3.cpp', 'grundfos_magna3.h', 'protocol.h',
    'telemetry.h', 'control_mode.h', 'operating_mode.h', 'setpoint.h',
    'number.py', 'sensor.py', 'select.py', 'switch.py', 'text_sensor.py', 'README.md',
]
test_files = [
    'protocol_test.cpp', 'telemetry_test.cpp', 'control_mode_test.cpp',
    'operating_mode_test.cpp', 'setpoint_test.cpp', 'setpoint_sequence_test.py',
]
files = [Path('components/grundfos_magna3') / f for f in component_files]
files += [Path('tests/grundfos_magna3') / f for f in test_files]
files += [Path(f) for f in [
    'LICENSE', 'docs/protocol.md', 'README.md', 'THIRD_PARTY_NOTICES.md', 'RELEASE_NOTES_0.1.0.md', '.gitignore',
    'examples/grundfos-magna3.yaml', '.github/workflows/test.yml',
    'scripts/test_host.sh', 'scripts/prepare_compile_example.py', 'scripts/prepare_release.py',
]]
# Do not silently erase an existing candidate or user edits inside it.
if destination.exists():
    raise SystemExit(f'{destination} already exists; inspect it before creating another candidate')
for relative in files:
    source = root / relative
    if not source.is_file() or source.is_symlink():
        raise SystemExit(f'Missing or symlinked source: {relative}')
for relative in files:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / relative, target)
actual = {p.relative_to(destination) for p in destination.rglob('*') if p.is_file()}
assert actual == set(files)
for relative in files:
    assert hashlib.sha256((root / relative).read_bytes()).digest() == hashlib.sha256((destination / relative).read_bytes()).digest()
print(f'Source candidate: {destination} ({len(files)} allowlisted files)')
print('MIT source bundle prepared; third-party notices included')
