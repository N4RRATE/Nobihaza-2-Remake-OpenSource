"""Two actual executable lifetimes, isolated saves, unchanged load bytes."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='nobi56 中文 saves ') as directory:
    exe,content=map(lambda s:str(Path(s).resolve()),sys.argv[1:3])
    subprocess.run([exe,content,directory,'write'],check=True,cwd=directory)
    before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in Path(directory).glob('slot-*.json')}
    subprocess.run([exe,content,directory,'load'],check=True,cwd=directory)
    after={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in Path(directory).glob('slot-*.json')}
    assert before==after,'Load changed saved progress'
    print('Separate write/load processes completed; original slot bytes unchanged')
