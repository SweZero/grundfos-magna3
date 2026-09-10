"""Use the public example unchanged except for its not-yet-published git source."""
from pathlib import Path
import yaml

root = Path(__file__).resolve().parents[1]
config = yaml.safe_load((root / "examples/grundfos-magna3.yaml").read_text())
config["external_components"] = [{
    "source": {"type": "local", "path": str(root / "components")},
    "components": ["grundfos_magna3"],
}]
output = root / ".ci/grundfos-magna3.yaml"
output.parent.mkdir(exist_ok=True)
output.write_text(yaml.safe_dump(config, sort_keys=False, allow_unicode=True))
print(output)
