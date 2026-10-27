"""Public reader must reject incomplete/unknown current content, not just old saves."""
import json,shutil,subprocess,sys,tempfile
from pathlib import Path
exe=Path(sys.argv[1]).resolve();source=Path(sys.argv[2]).resolve()
original=json.loads(source.read_text())
with tempfile.TemporaryDirectory(prefix='nobi-validation-') as tmp:
    root=Path(tmp)
    for path in source.parent.iterdir():
        if path.is_file() and path.name!=source.name:shutil.copyfile(path,root/path.name)
    candidate=root/source.name
    candidate.write_text(json.dumps(original,ensure_ascii=False))
    control=subprocess.run([str(exe),str(candidate)],capture_output=True,text=True)
    if control.returncode:raise AssertionError('Valid control rejected: '+control.stderr)
    cases=[('lighter-cue',lambda d:d['f4bc']['episodes']['lighter']['steps'][3].update(cue='missing-cue')),
           ('staff-room-death',lambda d:next(r for r in d['f4bc']['additional_enemies']['instances'] if r['id']=='m44-e2').update(dead_switch=9999)),
           ('canine-page',lambda d:next(r for r in d['f4bc']['enemy_instances'] if r['id']=='m32-e2')['page_roles']['pursuit'].update(page=9999)),
           ('missing-role',lambda d:d['f4bc']['zombie_profiles']['m27-e3'].pop('pursuit_page')),
           ('missing-fact',lambda d:d['f4bc']['state_contract']['facts'].pop()),
           ('wrong-switch',lambda d:d['f4bc']['state_contract']['facts'][-1].update(source_switch=9999)),
           ('unknown-fact',lambda d:d['f4bc']['state_contract']['facts'][-1].update(stable_id='unknown.fact')),
           ('unknown-capability',lambda d:d['capabilities']['weapons'].update({'99':'executable'})),
           ('outside-animation',lambda d:d['maps']['96']['animated_tiles'][0]['frame_rects_px'][0].__setitem__(0,99999))]
    for name,mutate in cases:
        data=json.loads(json.dumps(original));mutate(data);candidate.write_text(json.dumps(data,ensure_ascii=False))
        result=subprocess.run([str(exe),str(candidate)],capture_output=True,text=True)
        if result.returncode==0:raise AssertionError(name+' unexpectedly accepted')
        print(name,'rejected',flush=True)
print('B/C public reader validation checks passed')
