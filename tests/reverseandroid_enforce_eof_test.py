"""Independently exercise real shells against sel_read_enforce's byte format."""
import pathlib
import subprocess
import tempfile
import unittest


class EnforceEOFTest(unittest.TestCase):
    def test_original_read_and_complete_value_guard(self):
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp) / 'enforce'
            path.write_bytes(b'1')
            for shell in ['bash', 'dash']:
                original = subprocess.run([shell, '-c', 'set -e; IFS= read -r e < "$1"; test "$e" = 1', 'sh', str(path)])
                self.assertEqual(original.returncode, 1)
                observed = subprocess.run([shell, '-c', 'r=0; IFS= read -r e < "$1" || r=$?; printf "%s:%s" "$r" "$e"', 'sh', str(path)], capture_output=True, check=True)
                self.assertEqual(observed.stdout, b'1:1')
                for raw, expected in [(b'1', 0), (b'1\n', 0), (b'0', 1), (b'', 1), (b'11', 1)]:
                    path.write_bytes(raw)
                    result = subprocess.run([shell, '-c', 'set -e; e=$(cat "$1"); test "$e" = 1', 'sh', str(path)])
                    self.assertEqual(result.returncode, expected)
                path.write_bytes(b'1')


if __name__ == '__main__':
    unittest.main()
