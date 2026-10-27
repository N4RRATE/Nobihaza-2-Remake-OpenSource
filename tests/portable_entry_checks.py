"""Check the real executable's Mac bundle entry; optional full package stays isolated."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import selectors
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('--content', type=Path)
    parser.add_argument('--runtime-check', type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    with tempfile.TemporaryDirectory(prefix='nobihaza-portable-') as temporary:
        root = Path(temporary)
        bundle = root / '原位置' / 'Nobihaza 正式入口.app'
        binary = bundle / 'Contents/MacOS/nobihaza'
        binary.parent.mkdir(parents=True)
        shutil.copy2(executable, binary)
        (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps({
            'CFBundleExecutable': 'nobihaza', 'CFBundleIdentifier': 'local.nobihaza.portable-check',
            'CFBundlePackageType': 'APPL', 'CFBundleName': 'Nobihaza Entry Check'}))
        runtime = bundle / 'Contents/Resources/runtime'
        runtime.mkdir(parents=True)
        saves = root / '独立存档'
        saves.mkdir()
        sentinel = saves / 'slot-01.json'
        sentinel.write_bytes(b'preserve-existing-save')

        def run(target, options=(), expected=None, fake_argv=False):
            result = subprocess.run(
                ['unrelated-argv0' if fake_argv else str(target), *options],
                executable=str(target), cwd='/', capture_output=True, text=True,
                env={**os.environ, 'NOBIHAZA_STARTUP_PROFILE': '1'}, timeout=180)
            if expected is None:
                assert result.returncode == 0 and 'startup.first_frame_ms=' in result.stderr, result.stderr
            else:
                assert result.returncode == 1 and expected in result.stderr, result.stderr
            assert sentinel.read_bytes() == b'preserve-existing-save', 'startup modified existing save'
            assert sorted(p.name for p in saves.iterdir()) == ['slot-01.json'], 'startup wrote save files'
            return result.stderr

        run(executable, expected='requires a macOS .app bundle')
        run(binary, expected=str(runtime / 'content.json'), fake_argv=True)
        (runtime / 'content.json').write_text('{"schema":"nobihaza.g8late.v1"}', encoding='utf-8')
        run(binary, ['--frames', '1', '--save-dir', str(saves)], expected='Startup failed (scene:')
        if args.content:
            shutil.rmtree(runtime)
            shutil.copytree(args.content.resolve().parent, runtime)
            shutil.copy2(args.content.resolve(), runtime / 'content.json')
            options = ['--frames', '1', '--save-dir', str(saves)]
            run(binary, options, fake_argv=True)
            moved = root / '搬移 中文空格' / bundle.name
            moved.parent.mkdir()
            bundle.rename(moved)
            binary = moved / 'Contents/MacOS/nobihaza'
            runtime = moved / 'Contents/Resources/runtime'
            run(binary, options, fake_argv=True)
            if args.runtime_check:
                result = subprocess.run([str(args.runtime_check.resolve()), str(runtime / 'content.json'), str(saves)],
                                        cwd='/', capture_output=True, text=True, timeout=180)
                assert result.returncode == 0, result.stderr
                print(result.stdout.strip(), flush=True)
            # Exercise the exact no-argument player entry, stopping only this child after display.
            process = subprocess.Popen([str(binary)], cwd='/', stdout=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE, env={**os.environ, 'NOBIHAZA_STARTUP_PROFILE': '1'})
            output = b''
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(process.stderr, selectors.EVENT_READ)
                    deadline = time.monotonic() + 180
                    while b'startup.first_frame_ms=' not in output and time.monotonic() < deadline:
                        if selector.select(timeout=1):
                            chunk = os.read(process.stderr.fileno(), 4096)
                            if not chunk:
                                break
                            output += chunk
                assert b'startup.first_frame_ms=' in output, output.decode(errors='replace')
            finally:
                if process.poll() is None:
                    process.terminate()
                process.communicate(timeout=10)
            print('exact no-argument bundle displayed its first title frame', flush=True)
            # Explicit content and options retain the development path in a bundle.
            run(binary, [str(runtime / 'content.json'), '--entry', 'title', *options])
            original = (runtime / 'content.json').read_text(encoding='utf-8')
            bad = json.loads(original)
            bad['source']['sha256'] = '0' * 64
            (runtime / 'content.json').write_text(json.dumps(bad), encoding='utf-8')
            run(binary, options, expected='Unknown late difficulty predecessor')
            (runtime / 'content.json').write_text(original, encoding='utf-8')
            (runtime / 'font.otf').rename(runtime / 'hidden-font.otf')
            run(binary, options, expected='font.otf')
            (runtime / 'hidden-font.otf').rename(runtime / 'font.otf')
            (runtime / 'content.json').unlink()
            run(binary, options, expected=str(runtime / 'content.json'))
        print('portable entry checks passed' + (' (full G8 package)' if args.content else ' (failure boundaries)'))


if __name__ == '__main__':
    main()
