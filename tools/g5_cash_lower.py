"""Reviewed CN1.00 fixed Cash pages into explicit C++ branches; no runtime array VM."""
import argparse,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];SOURCE=ROOT/'private/generated/g5/01-prepared/g5-prepared.json'
def generate():
 from public_source_lower import lower_cash
 raw = SOURCE.read_bytes()
 review = json.loads((ROOT / 'tools/g5-cash-runtime-review.json').read_text())
 assert hashlib.sha256(raw).hexdigest() == review['prepared_sha256'], 'Changed reviewed Cash source'
 return lower_cash(raw)
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');a=parser.parse_args();target=ROOT/'src/game/cash_program.inc';s=generate()
 if a.check:assert target.read_text()==s,'Changed Cash compiled pages'
 else:target.write_text(s)
 print('Cash fixed lowering reproducible'if a.check else'Cash fixed pages lowered')
