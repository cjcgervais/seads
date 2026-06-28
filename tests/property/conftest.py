import sys
from pathlib import Path

# Make the canonical reference modules importable.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
