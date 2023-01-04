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
        reset("/Users/joslin/amaxfactory/log/dex.log")
    
    
    @classmethod
    def tearDownClass(cls):
        time.sleep(60000)
        stop()
        
    def test_register(self):
        '''The only test function.

        The account objects `master, host, alice, ...` which are of the global namespace, do not have to be explicitly declared (and still keep the linter silent).
        '''
        
        
        dex_path = "/Users/joslin/code/workspace/opensource/dex.contracts"
        init.build(dex_path)
        dex = init.ORDERBOOKDEX()
        dex.init(dex)
        dex.get_config(dex)
        
        COMMENT('''
        finished
        ''')
        master = new_master_account()
        
        buyerpp1 = new_account(master, "buyerpp1")
        buyerp1 = new_account(buyerpp1, "buyerp1")
        
        sellerpp1 = new_account(master, "sellerpp1")
        sellerp1 = new_account(sellerpp1, "sellerp1")
        
        
        amax_token  = init.deploy_amax()
        amax_mtoken = init.deploy_mtoken()
        farm = init.deploy_farm()
        
        COMMENT('''
        Create test accounts:
        ''')
        
        dexadmin = new_account(master,"dexadmin")
        admin = new_account(master,"admin")
        
        dex.setsympair(["8,METH", "amax.mtoken"], ["6,MUSDT", "amax.mtoken"], "0.01000000 METH","1.000000 MUSDT", True, dexadmin )
        dex.get_sympair(dex)

        
        buyer = self.init_buyer_account(buyerp1, admin, amax_token, amax_mtoken, "buyer")
        
        seller = self.init_seller_account(sellerp1, admin, amax_token, amax_mtoken, "seller")
        # u3 = self.init_account(master, admin, amax_token, amax_mtoken, "u3")
        # u4 = self.init_account(master, admin, amax_token, amax_mtoken, "u4")
        
        COMMENT('''
         add order 
        ''')
        # orderbookdex.push_action("neworder", ["u1", 1, "buy", "0.01000000  METH","0.01000000  METH", "100.000000 MUSDT", 2, None ], u1)
        # table_gloab = orderbookdex.table("queue", "orderbookdex")
        
        dex.neworder("seller", 1, "sell", "0.01000000  METH","300.000000 MUSDT", 3, None, seller)
        seller.transfer(dex, "0.01000000  METH", "")
        time.sleep(1)
        
        dex.neworder("buyer", 1, "buy", "0.01000000  METH", "400.000000 MUSDT", 2, None, buyer)
        buyer.transfer(dex, "4.000000 MUSDT", "")
        time.sleep(1)
        
  
        
        # dex.withdraw("dexadmin", "amax.mtoken", "0.00003000 METH", None, dexadmin)
        # dex.withdraw("dexadmin", "amax.mtoken", "0.008986 MUSDT", None, dexadmin)
        # dex.withdraw("sellerp1", "amax.mtoken", "0.000001 MUSDT", None, sellerp1)
        # time.sleep(1)
        # dex.withdraw("sellerp1", "amax.mtoken", "0.000001 MUSDT", None, sellerp1)
        # time.sleep(1)
        
        # dex.withdraw("sellerp1", "amax.mtoken", "0.000001 MUSDT", None, sellerp1)
        # time.sleep(1)
        
        COMMENT('''
            finished
        ''')
        # dex.neworder("buyer", 1, "buy", "0.02000000  METH", "200.000000 MUSDT", 2, None, buyer)
        # buyer.transfer(dex, "4.012000 MUSDT", "")
        # time.sleep(1)
        
        
        
        # dex.neworder("seller", 1, "sell", "0.00500000  METH", "80.000000 MUSDT", 4, None, seller)
        # seller.transfer(dex, "0.00500000  METH", "")
        # time.sleep(1)
        
        
        # dex.neworder("u1", 1, "sell", "0.00600000  METH","0.01000000  METH", "100.000000 MUSDT", 4, None, u1)
        # u1.transfer(dex, "0.00600000  METH", "")
        # time.sleep(1)
        
        
        # dex.neworder("u1", 1, "sell", "0.00600000  METH","0.01000000  METH", "100.000000 MUSDT", 4, None, u1)
        # u1.transfer(dex, "0.00600000  METH", "")  
        # time.sleep(1)
        
        
        # dex.neworder("u1", 1, "sell", "0.01000000  METH","0.01000000  METH", "101.000000 MUSDT", 4, None, u1)
        # u1.transfer(dex, "0.01000000  METH", "")
        # time.sleep(1)
        # dex.neworder("u1", 1, "sell", "0.00500000  METH","0.01000000  METH", "100.000000 MUSDT", 4, None, u1)
        # u1.transfer(dex, "0.01000000  METH", "")
        
        
        COMMENT('''
        finished
        ''')
        time.sleep(1)
        
    def init_buyer_account(self, p1, admin, amax_token, amax_mtoken, user):
        a = new_account(p1, user)
        admin.transfer(a, "20.000000 MUSDT", "")
        return a
    
    def init_seller_account(self, p1, admin, amax_token, amax_mtoken, user):
        a = new_account(p1, user)
        admin.transfer(a, "20.00000000 AMAX", "")
        admin.transfer(a, "20.00000000 METH", "")
        admin.transfer(a, "20.00000000 MBTC", "")
        return a   
    
if __name__ == "__main__":
    unittest.main()
