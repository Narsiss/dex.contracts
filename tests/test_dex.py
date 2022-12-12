import time
import unittest
from amaxfactory.eosf import *

verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.TRACE, Verbosity.DEBUG])

CONTRACT_WASM_PATH = "/Users/joslin/code/workspace/amaxfactory/templates/wasm/"

CUSTOMER_WASM_PATH = "/Users/joslin/code/workspace/opensource/dex.contracts"

class Test(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        SCENARIO('''
        Create a contract from template, then build and deploy it.
        ''')
        reset()
    
    
    @classmethod
    def tearDownClass(cls):
        # time.sleep(60)
        stop()
        
    def test_register(self):
        '''The only test function.

        The account objects `master, host, alice, ...` which are of the global namespace, do not have to be explicitly declared (and still keep the linter silent).
        '''
        
        master = new_master_account()
        
        # init.deploy_amax()
        # init.deploy_mtoken()
        
        COMMENT('''
        Create test accounts:
        ''')

        orderbookdex = new_account(master,"c")
        solotestacct = new_account(master,"solotestacct")
        
        # farm = init.deploy_farm()


        smart = Contract(orderbookdex, 
                wasm_file=CUSTOMER_WASM_PATH + '/build/contracts/orderbookdex/orderbookdex.wasm',
                abi_file=CUSTOMER_WASM_PATH + '/build/contracts/orderbookdex/orderbookdex.abi')
        smart.deploy()

        orderbookdex.push_action( "init", {})
        
        table_gloab = orderbookdex.table("config")
        print(table_gloab)
        
        
        
        
if __name__ == "__main__":
    unittest.main()
