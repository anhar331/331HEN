import unittest
from scripts.installer_source import identity,validate_item
class InstallerTests(unittest.TestCase):
    def item(self):
        v='gamepatch-PPSA26344-v1.0-331.1';n,f,d=identity(v);u=f'https://github.com/anhar331/331HEN/releases/download/{v}/{f}'
        return {'name':n,'version':v,'filename':f,'url':u,'source_direct':u,'description':d,'checksum':'0'*64}
    def test_patch_installer_is_a_separate_family_and_explicitly_untested(self):
        p=self.item();validate_item(p);self.assertIn('NOT PS5-tested',p['description']);self.assertNotEqual(p['name'],'331HEN')
    def test_cannot_use_hen_name_or_asset(self):
        p=self.item();p['name']='331HEN'
        with self.assertRaises(ValueError):validate_item(p)
        p=self.item();p['url']=p['url'].rsplit('/',1)[0]+'/331HEN.elf'
        with self.assertRaises(ValueError):validate_item(p)
    def test_unknown_game_installer_refused(self):
        with self.assertRaises(ValueError):identity('gamepatch-PPSA00000-v1.0-331.1')
