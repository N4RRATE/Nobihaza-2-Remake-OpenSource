"""F3 writes and exits; F4 title Load in a different process; optional real copies."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
exe, content = (str(Path(p).resolve()) for p in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='nobi64 legacy 中文 ') as directory:
    root = Path(directory)
    if len(sys.argv) == 5:
        for slot, source in enumerate(sys.argv[3:5], 1):
            shutil.copyfile(source, root / f'slot-{slot:02}.json')
    else:
        subprocess.run([exe, content, directory, 'write'], check=True, cwd=directory)
    before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir()}
    subprocess.run([exe, content, directory, 'load'], check=True, cwd=directory)
    assert before == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir()}
    print('Old F3 slot bytes unchanged after independent F4 title Load')
