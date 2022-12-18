import time
import unittest
from amaxfactory.eosf import *

verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.TRACE, Verbosity.DEBUG])

CONTRACT_WASM_PATH = "/Users/joslin/code/workspace/amaxfactory/templates/wasm/"

CUSTOMER_WASM_PATH = "/Users/joslin/code/workspace/opensource/dex.contracts"

MASTER = MasterAccount()
HOST = Account()

class Test(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        SCENARIO('''
        Create a contract from template, then build and deploy it.
        ''')
        reset("/tmp/amaxfactory/log/dex.log")
    
    
    @classmethod
    def tearDownClass(cls):
        time.sleep(60000)
        stop()
        
    def test_register(self):
        '''The only test function.

        The account objects `master, host, alice, ...` which are of the global namespace, do not have to be explicitly declared (and still keep the linter silent).
        '''
        
        
        dex_path = "/Users/joslin/code/workspace/opensource/dex.contracts"
        # init.build(dex_path)
        dex = init.ORDERBOOKDEX()
        dex.init(dex)
        dex.get_config(dex)
        
        COMMENT('''
        finished
        ''')
        
        master = new_master_account()
        amax_token  = init.deploy_amax()
        amax_mtoken = init.deploy_mtoken()
        farm = init.deploy_farm()
        
        COMMENT('''
        Create test accounts:
        ''')
        
        dexadmin = new_account(master,"dexadmin")
        admin = new_account(master,"admin")
        
        dex.setsympair(["8,METH", "amax.mtoken"], ["6,MUSDT", "amax.mtoken"], "0.01000000 METH","1.000000 MUSDT", True, True, dexadmin )
        dex.get_sympair(dex)

        
        u1 = self.init_account(master, admin, amax_token, amax_mtoken, "u1")
        u2 = self.init_account(master, admin, amax_token, amax_mtoken, "u2")
        # u3 = self.init_account(master, admin, amax_token, amax_mtoken, "u3")
        # u4 = self.init_account(master, admin, amax_token, amax_mtoken, "u4")
        
        COMMENT('''
         add order 
        ''')
        # orderbookdex.push_action("neworder", ["u1", 1, "buy", "0.01000000  METH","0.01000000  METH", "100.000000 MUSDT", 2, None ], u1)
        # table_gloab = orderbookdex.table("queue", "orderbookdex")
        
        dex.neworder("u1", 1, "buy", "0.01000000  METH","0.01000000  METH", "100.000000 MUSDT", 2, None, u1)
        u1.transfer(dex, "1.003000 MUSDT", "")
        time.sleep(1)
        
        dex.neworder("u1", 1, "sell", "0.01000000  METH","0.01000000  METH", "200.000000 MUSDT", 3, None, u1)
        u1.transfer(dex, "0.01000000  METH", "")
        time.sleep(1)
        
        dex.neworder("u1", 1, "sell", "0.00500000  METH","0.01000000  METH", "100.000000 MUSDT", 4, None, u1)
        u1.transfer(dex, "0.00500000  METH", "")
        time.sleep(1)
        
        dex.neworder("u1", 1, "sell", "0.00600000  METH","0.01000000  METH", "100.000000 MUSDT", 4, None, u1)
        u1.transfer(dex, "0.00600000  METH", "")
        time.sleep(1)
        
        # dex.neworder("u1", 1, "sell", "0.01000000  METH","0.01000000  METH", "101.000000 MUSDT", 4, None, u1)
        # u1.transfer(dex, "0.01000000  METH", "")
        # time.sleep(1)
        # dex.neworder("u1", 1, "sell", "0.00500000  METH","0.01000000  METH", "100.000000 MUSDT", 4, None, u1)
        # u1.transfer(dex, "0.01000000  METH", "")
        
        
        COMMENT('''
        finished
        ''')
        time.sleep(1)
        
    def init_account(self, master, admin, amax_token, amax_mtoken, user):
        a = new_account(master, user)
        admin.transfer(a, "20.00000000 AMAX", "")
        admin.transfer(a, "20.00000000 METH", "")
        admin.transfer(a, "20.00000000 MBTC", "")
        amax_mtoken.table("accounts", admin)
        amax_mtoken.table("accounts", a)
        amax_token.table("accounts", a)
        admin.transfer(a, "20.000000 MUSDT", "")
        amax_mtoken.table("accounts", a)
        
        
        return a
        
if __name__ == "__main__":
    unittest.main()
