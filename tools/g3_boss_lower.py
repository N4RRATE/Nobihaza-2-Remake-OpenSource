"""CN1.00 fixed Dekisugi pages -> explicit C++ control flow, never a runtime VM."""
import argparse,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/'private/generated/g3/01/g3-source-evidence.json'
def generate():
 from public_source_lower import lower_dekisugi
 raw = EVIDENCE.read_bytes()
 review = json.loads((ROOT / 'tools/g3-boss-review.json').read_text())
 assert hashlib.sha256(raw).hexdigest() == review['source_evidence_sha256'], 'Changed reviewed source'
 return lower_dekisugi(raw)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args();target=ROOT/'src/game/dekisugi_program.inc';s=generate()
 if a.check:assert target.read_text()==s,'Dekisugi generated control flow differs'
 else:target.write_text(s)
 print('fixed Dekisugi pages lowered' if not a.check else 'fixed lowering reproducible')
