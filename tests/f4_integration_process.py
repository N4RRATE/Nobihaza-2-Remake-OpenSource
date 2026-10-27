"""Actual independent processes and cwd; byte-preserving Load in 24 isolated slots."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile
exe, content = (str(Path(p).resolve()) for p in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='nobi64 中文 isolated ') as directory:
    subprocess.run([exe, content, directory, 'write'], cwd=directory, check=True)
    def hashes():
        return {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in Path(directory).rglob('*.json')}
    before = hashes()
    expected = int(sys.argv[3]) if len(sys.argv) > 3 else 24
    assert len(before) == expected
    subprocess.run([exe, content, directory, 'load'], cwd=directory, check=True)
    assert before == hashes(), 'Load changed saved bytes'
    print(f'{expected} slots across two save contexts: separate process Load preserved bytes')
