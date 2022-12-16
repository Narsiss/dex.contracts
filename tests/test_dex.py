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
        
        master = new_master_account()
        
        amax_token  = init.deploy_amax()
        amax_mtoken = init.deploy_mtoken()
        
        COMMENT('''
        Create test accounts:
        ''')

        orderbookdex = new_account(master,"orderbookdex")
        dexadmin = new_account(master,"dexadmin")
        
        # farm = init.deploy_farm()

        smart = Contract(orderbookdex, 
                wasm_file=CUSTOMER_WASM_PATH + '/build/contracts/orderbookdex/orderbookdex.wasm',
                abi_file=CUSTOMER_WASM_PATH + '/build/contracts/orderbookdex/orderbookdex.abi')
        smart.deploy()
        orderbookdex.set_account_permission(add_code=True)


        orderbookdex.push_action( "init", {})
        table_gloab = orderbookdex.table("config", "orderbookdex")
        print(table_gloab)
        
        COMMENT('''
        add :
        ''')
        orderbookdex.push_action( "setsympair",[["8,METH", "amax.mtoken"],["6,MUSDT", "amax.mtoken"],"0.01000000 METH", "1.000000 MUSDT", True, True ], 
                        dexadmin)
        
        table_gloab = orderbookdex.table("sympair", "orderbookdex")
        
        admin = new_account(master,"admin")

        u1 = self.init_account(master, admin, amax_token, amax_mtoken, "u1")
        u2 = self.init_account(master, admin, amax_token, amax_mtoken, "u2")
        u3 = self.init_account(master, admin, amax_token, amax_mtoken, "u3")
        u4 = self.init_account(master, admin, amax_token, amax_mtoken, "u4")

        #  const asset &limit_quant, const asset &frozen_quant,
        #  const asset &price, const uint64_t &external_id,
             
             
        COMMENT('''
         add order 
        ''')
        orderbookdex.push_action("neworder", ["u1", 1, "buy", "0.01000000  METH","0.01000000  METH", "100.000000 MUSDT", 2, None ], u1)
        table_gloab = orderbookdex.table("queue", "orderbookdex")
        
        u1.transfer(orderbookdex, "1.003000 MUSDT", "")
        
        COMMENT('''
        finished
        ''')
        time.sleep(1)
        
    def init_account(self, master, admin, amax_token, amax_mtoken, user):
        a = new_account(master, user)
        admin.transfer(a, "20.00000000 AMAX", "")
        admin.transfer(a, "20.00000000 METH", "")
        amax_mtoken.table("accounts", admin)
        amax_mtoken.table("accounts", a)
        amax_token.table("accounts", a)
        admin.transfer(a, "20.000000 MUSDT", "")
        amax_mtoken.table("accounts", a)
        
        
        return a
        
if __name__ == "__main__":
    unittest.main()
