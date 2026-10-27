"""Reviewed CN1.00 fixed Dora pages into explicit C++ branches; no runtime array VM."""
import argparse,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];SOURCE=ROOT/'private/generated/g4/02-prepared/g4-prepared.json'
def generate():
 from public_source_lower import lower_dora
 raw = SOURCE.read_bytes()
 review = json.loads((ROOT / 'tools/g4-dora-runtime-review.json').read_text())
 assert hashlib.sha256(raw).hexdigest() == review['prepared_sha256'], 'Changed reviewed Dora source'
 return lower_dora(raw)
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');a=parser.parse_args();target=ROOT/'src/game/dora_program.inc';s=generate()
 if a.check:assert target.read_text()==s,'Changed Dora compiled pages'
 else:target.write_text(s)
 print('Dora fixed lowering reproducible'if a.check else'Dora fixed pages lowered')
