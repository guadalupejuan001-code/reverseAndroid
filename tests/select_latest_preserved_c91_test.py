import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("selector",ROOT/"scripts/select_latest_preserved_c91.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class SelectorTest(unittest.TestCase):
    def test_groups_only_complete_name_shape(self):
        a=[
          {"name":"reverseandroid-c91-7-metadata","expired":False},
          {"name":"reverseandroid-c91-7-000","expired":False},
          {"name":"reverseandroid-c91-7-x","expired":False},
          {"name":"reverseandroid-c91-8-metadata","expired":True},
        ]
        g=m.group_artifacts(a)
        self.assertIn(7,g);self.assertIn("metadata",g[7]);self.assertIn("000",g[7])
        self.assertNotIn(8,g)

    def test_candidate_requires_every_manifest_piece(self):
        def item(name):
            return {"id":1,"name":name,"digest":"sha256:"+"a"*64,"expired":False,
                    "workflow_run":{"id":m.RUN_ID,"head_sha":"a035f386b468c71ad11c32aee88252dbab0b91db"}}
        manifest={"schema":"reverseandroid-checkpoint-transport-v1","compression":"tar+gzip",
          "source_contract":{"checkpoint_sha256":"b"*64,"instructions":1,
             "source_commit":"a035f386b468c71ad11c32aee88252dbab0b91db"},
          "progress":{"checkpoint_sha256":"b"*64,"instructions":1,"source_boundary_verified":True,
             "system_server_alive_verified":True,"android_boot_completed_verified":False},
          "parts":[{"file":"checkpoint.part-000","bytes":100,"sha256":"c"*64},
                   {"file":"checkpoint.part-001","bytes":100,"sha256":"d"*64}]}
        group={"metadata":item("reverseandroid-c91-7-metadata"),"000":item("reverseandroid-c91-7-000")}
        with self.assertRaises(AssertionError):m.validate_candidate(7,group,manifest)
        group["001"]=item("reverseandroid-c91-7-001")
        c,p,pieces,meta=m.validate_candidate(7,group,manifest)
        self.assertEqual(len(pieces),2);self.assertEqual(c["checkpoint_sha256"],"b"*64)

if __name__=="__main__":unittest.main()
