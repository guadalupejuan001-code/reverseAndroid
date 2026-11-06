"""Reject shell echoes, partial responses and ambiguous actual replies."""
import pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scripts'))
from c89_isolated_anr_read import parse_response

class ReadResponseTest(unittest.TestCase):
    def test_only_complete_ordered_real_lines_finish_the_read_query(self):
        self.assertFalse(parse_response(b"$ printf 'REVERSE_ANR_READ_BEGIN'; echo REVERSE_ANR_READ_END RC_0\n")['completed_read_query'])
        self.assertFalse(parse_response(b'REVERSE_ANR_READ_BEGIN\ntrace still arriving\n')['completed_read_query'])
        self.assertFalse(parse_response(b'REVERSE_ANR_READ_BEGIN\nREVERSE_ANR_READ_END RC_')['completed_read_query'])
        self.assertTrue(parse_response(b'\x1b[32mREVERSE_ANR_READ_BEGIN\r\nactual trace\r\nREVERSE_ANR_READ_END RC_0\r\n')['completed_read_query'])

    def test_nonzero_duplicate_or_unordered_replies_cannot_pass(self):
        result=parse_response(b'REVERSE_ANR_READ_BEGIN\nREVERSE_ANR_READ_END RC_1\n')
        self.assertTrue(result['failed']);self.assertFalse(result['completed_read_query'])
        for raw in [b'REVERSE_ANR_READ_END RC_0\n',b'REVERSE_ANR_READ_END RC_0\nREVERSE_ANR_READ_BEGIN\n',b'REVERSE_ANR_READ_BEGIN\nREVERSE_ANR_READ_BEGIN\n',b'REVERSE_ANR_READ_BEGIN\nREVERSE_ANR_READ_END RC_0\nREVERSE_ANR_READ_END RC_0\n']:
            with self.subTest(raw=raw),self.assertRaises(AssertionError):parse_response(raw)

if __name__=='__main__':unittest.main()
