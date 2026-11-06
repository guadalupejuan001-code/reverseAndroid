import copy,pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scripts'))
from zero_input_continuation import validate

class ZeroInputContinuation(unittest.TestCase):
    def test_safe_plan_and_reject_new_input_path_and_unbounded_work(self):
        p=dict(schema='reverseandroid-original-zero-input-continuation-plan-v1',cycle='c89',new_UART_bytes=0,
               host_wall_ms=900000,max_stages=16,initial_instruction_budget=1000000000,continuation_instruction_budget=1000000000,
               inherited_source_events=[{'phase':i} for i in range(8)],
               expected_native_SHA256='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f',
               source_checkpoint_sha256='0'*64,source_instructions=1)
        validate(p)
        for key,value in [('new_UART_bytes',1),('input_file','old-command.txt'),('input_plan_file','old-command.json'),
                          ('cycle','../old-source'),('initial_instruction_budget',2000000000),('continuation_instruction_budget',0),
                          ('host_wall_ms',900001),('inherited_source_events',[{'phase':1}])]:
            bad=copy.deepcopy(p);bad[key]=value
            with self.subTest(key=key),self.assertRaises(AssertionError):validate(bad)

if __name__=='__main__':unittest.main()
