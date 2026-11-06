import hashlib,pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scripts'))
from ui_response_control import parse_ui_reply,native_pixels,frame_difference

class UIResponseControl(unittest.TestCase):
    def test_echo_partial_and_error_are_not_success(self):
        for raw in [b"printf 'REVERSE_UI_RESPONSE RC_0'\n",b'REVERSE_UI_RESPONSE RC_\n',b'REVERSE_UI_RESPONSE RC_41\n']:
            self.assertFalse(parse_ui_reply(raw)['command_succeeded'])
        self.assertTrue(parse_ui_reply(b'REVERSE_UI_RESPONSE RC_41\n')['command_failed'])
    def test_one_real_exact_reply(self):
        raw=b'\x1b[0mREVERSE_UI_RESPONSE RC_0\r\n'
        self.assertTrue(parse_ui_reply(raw)['command_succeeded'])
        with self.assertRaises(AssertionError):parse_ui_reply(raw+raw)
    def test_original_pixel_authentication_and_difference_do_not_claim_meaning(self):
        def source(pixels):
            raw=b'P6\n1 1\n255\n'+pixels
            return raw,dict(width=1,height=1,ppm_sha256=hashlib.sha256(raw).hexdigest(),rgb_sha256=hashlib.sha256(pixels).hexdigest())
        a,af=source(b'\n\x00\x01');b,bf=source(b'\n\x00\x02')
        self.assertEqual(native_pixels(a,af),b'\n\x00\x01')
        result=frame_difference(a,af,b,bf);self.assertEqual(result['changed_pixels'],1)
        self.assertTrue(result['authentic_frame_changed']);self.assertFalse(result['meaningful_UI_response_manually_verified'])
        self.assertFalse(frame_difference(a,af,a,af)['authentic_frame_changed'])
        with self.assertRaises(AssertionError):native_pixels(a+b'x',af)

if __name__=='__main__':unittest.main()
