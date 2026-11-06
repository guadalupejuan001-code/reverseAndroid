import copy,pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scripts'))
from c88_jit_priority_control import parse_reply,validate_plan,read

class Control(unittest.TestCase):
    def test_console_echo_and_partial_reply_are_not_success(self):
        self.assertFalse(parse_reply(b"sh -c 'printf REVERSE_JIT5922_CONTROL_RESULT RC_0'\n")['guard_succeeded'])
        self.assertFalse(parse_reply(b'REVERSE_JIT5922_CONTROL_RESULT RC_0\n')['guard_succeeded'])
        self.assertFalse(parse_reply(b'REVERSE_JIT5922_CONTROL_DONE NICE_0\n')['guard_succeeded'])
    def test_real_exact_reply_and_nonzero_failure(self):
        raw=b'\x1b[0mREVERSE_JIT5922_CONTROL_DONE NICE_0\r\nREVERSE_JIT5922_CONTROL_RESULT RC_0\r\n'
        self.assertTrue(parse_reply(raw)['guard_succeeded'])
        self.assertTrue(parse_reply(b'REVERSE_JIT5922_CONTROL_RESULT RC_43\n')['guard_failed'])
        with self.assertRaises(AssertionError):parse_reply(raw+raw)
    def test_host_budget_and_source_ledger_are_not_relaxed(self):
        p=read(pathlib.Path(__file__).resolve().parents[1]/'state/experiments/c88-plan.json');validate_plan(p)
        for key,value in [('initial_instruction_budget',2000000000),('continuation_instruction_budget',0),('host_wall_ms',1200000),('target_JIT_TID',1),('inherited_source_events',[])]:
            bad=copy.deepcopy(p);bad[key]=value
            with self.subTest(key=key),self.assertRaises(AssertionError):validate_plan(bad)

if __name__=='__main__':unittest.main()
