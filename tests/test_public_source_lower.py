"""Synthetic rejection checks; no original command arrays or dialogue included."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('lower', Path(__file__).resolve().parents[1] / 'tools/public_source_lower.py')
lower = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lower)


class LowerChecks(unittest.TestCase):
    def test_supported_command_and_unknown_operation(self):
        for code in (10, 999999):
            evidence = {'maps': {'165': {'sha256': 'synthetic', 'events': [{'id': 2, 'pages': [{'id': 1, 'commands': [{'code': code, 'parameters': [], 'indent': 0}]}]}]}}}
            raw = json.dumps(evidence).encode()
            if code == 10:
                self.assertIn('f.next=1;', lower.lower_dekisugi(raw))
            else:
                with self.assertRaises(ValueError):
                    lower.lower_dekisugi(raw)

    def test_unsupported_operand_rejects_even_without_python_asserts(self):
        commands = [{'code': 10220, 'parameters': [0, 1, 1, 0, 999, 0], 'indent': 0}]
        prepared = {'projection': {'source_pages': {'m192-e2-p1': commands, 'ce9': []}}}
        with self.assertRaises(ValueError):
            lower.lower_dora(json.dumps(prepared).encode())

    def test_projection_shape_and_delimiter(self):
        value = dict.fromkeys(('objects', 'flows', 'cues', 'messages', 'sprites'), {})
        text = lower.correction(json.dumps(value).encode(), 'doors')
        self.assertIn('area3DoorCorrection', text)
        with self.assertRaises(ValueError):
            lower.correction(b'{}', 'doors')
        value['messages'] = {'synthetic': ')g3doors"'}
        escaped = lower.correction(json.dumps(value).encode(), 'doors')
        self.assertIn(')g3doors\\\"', escaped)

    def test_wrong_missing_changed_or_linked_inputs_leave_output_absent(self):
        contract = json.loads(Path(lower.__file__).with_name('public-compile-contract.json').read_text())
        with tempfile.TemporaryDirectory() as name:
            root = Path(name).resolve()
            output = root / 'generated'
            manifest = root / 'inputs.json'
            inputs = {key: {'file': 'missing.json', 'sha256': lock['sha256']} for key, lock in contract['inputs'].items()}
            for mode in ('missing', 'changed', 'symlink', 'unknown-set', 'wrong-hash'):
                with self.subTest(mode=mode):
                    data = {'schema': 'nobihaza.local-compile-inputs.v1', 'inputs': json.loads(json.dumps(inputs))}
                    source = root / 'missing.json'
                    source.unlink(missing_ok=True)
                    if mode == 'changed':
                        source.write_text('{}')
                    if mode == 'symlink':
                        source.symlink_to(manifest)
                    if mode == 'unknown-set':
                        data['inputs']['extra'] = {}
                    if mode == 'wrong-hash':
                        data['inputs']['dekisugi']['sha256'] = '0' * 64
                    manifest.write_text(json.dumps(data))
                    with self.assertRaises((ValueError, OSError)):
                        lower.generate(manifest, output)
                    self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
