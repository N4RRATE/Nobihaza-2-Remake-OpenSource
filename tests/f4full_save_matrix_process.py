"""106: independent executable lifetimes; never touches formal saves."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def hashes(directory):
    return {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in directory.rglob("slot-*.json")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("package", type=Path)
    parser.add_argument("--legacy", type=Path, required=True)
    parser.add_argument("--training-package", type=Path, required=True)
    parser.add_argument("--upgrade-package", type=Path, action="append", required=True)
    parser.add_argument("--reader-package", type=Path, action="append", default=[])
    parser.add_argument("--phase", choices=("all", "matrix", "upgrade"), default="all")
    args = parser.parse_args()
    executable, package = args.executable.resolve(), args.package.resolve()
    readers = [package] + [p.resolve() for p in args.reader_package]
    training = args.training_package.resolve()
    upgrades = [p.resolve() for p in args.upgrade_package]
    stages = {json.loads(p.read_text())["stage"] for p in upgrades}
    assert {"underground", "parking", "truth"} <= stages, "All intermediate cumulative stages must read the training disk"
    assert json.loads(training.read_text())["stage"] == "training", "Early writer must be the actual training stage"
    if args.legacy:
        source = args.legacy.resolve()
        original = source.read_bytes()
    with tempfile.TemporaryDirectory(prefix="nobi106 中文 isolated ") as name:
        root = Path(name)
        if args.phase in ("all", "matrix"):
            if args.legacy:
                (root / "legacy").mkdir()
                shutil.copyfile(source, root / "legacy" / "slot-03.json")
            subprocess.run([str(executable), str(package), str(root), "write"], check=True, cwd=root)
            before = hashes(root)
            assert len(list(root.glob("slot-*.json"))) == 4, "All four real save points must write"
            for reader in readers:
                mode = "final-load" if reader == package else "load"
                subprocess.run([str(executable), str(reader), str(root), mode], check=True, cwd=root)
                assert hashes(root) == before, "Load changed isolated save bytes"
        if args.phase in ("all", "upgrade"):
            early = root / "training-upgrade"
            subprocess.run([str(executable), str(training), str(early), "training-write"], check=True, cwd=root)
            early_bytes = hashes(early)
            assert len(early_bytes) == 1, "One real early-stage disk is the upgrade source"
            for reader in [training] + upgrades + [package]:
                subprocess.run([str(executable), str(reader), str(early), "training-load"], check=True, cwd=root)
                assert hashes(early) == early_bytes, "Cumulative stage Load rewrote the early disk"
        if args.legacy:
            assert source.read_bytes() == original, "Original evidence changed"
    print(f"106 {args.phase} separate write/load processes passed; slots and legacy evidence unchanged")


if __name__ == "__main__":
    main()
