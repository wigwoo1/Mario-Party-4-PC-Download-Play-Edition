import tempfile
from pathlib import Path
import unittest
from browser_module_manifest import inventory

class InventoryTests(unittest.TestCase):
    def test_pc_non_japanese_selection_and_case_matching(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); (root/'include').mkdir(); (root/'src/REL/bootDll').mkdir(parents=True)
            (root/'src/REL/bootDll/main.c').write_text('void ObjectSetup(void) {}')
            (root/'files.cmake').write_text('set(REL_FILES src/REL/bootDll/main.c)')
            (root/'include/ovl_table.h').write_text('#ifdef TARGET_PC\nDLL(bootdll)\n#if VERSION_JP\nDLL(japan)\n#else\nDLL(usa)\n#endif\n#else\nDLL(console)\n#endif\n')
            report=inventory(root)
            self.assertEqual([m['name'] for m in report['modules']],['bootdll','usa'])
            self.assertEqual(report['modules'][0]['sources'],['src/REL/bootDll/main.c'])
            self.assertEqual(report['modules'][1]['overlay_id'],1)
            self.assertEqual(report['unmapped_modules'],[])
            self.assertEqual(report['modules'][1]['source_status'],'no_c_source_group')
            (root/'src/REL/usa').mkdir()
            (root/'src/REL/usa/main.c').write_text('void ObjectSetup(void) {}')
            updated=inventory(root)['modules'][1]
            self.assertEqual(updated['source_status'],'source_not_in_build')
            self.assertEqual(updated['unlisted_sources'],['src/REL/usa/main.c'])

if __name__=='__main__': unittest.main()
