d=dexdexdexde2
a=dexadmin
u=u1

# tnew $d

tset $d orderbookdex

tcli set account permission $d active --add-code


tnew u1
 tcli push action amax.mtoken transfer '["user1","'$u'","10.000000 MUSDT","buy:1:1"]' -p user1

tcli push action $d init '[]' -p $d

tcli get table $d $d config

tcli get table $d $d sympair


tcli push action $d setsympair '[["8,METH", "amax.mtoken"],["6,MUSDT", "amax.mtoken"],"0.01000000 METH", "1.000000 MUSDT",true, true ]' -p $a

tcli push action $d delsympair '[1]' -p $a

tcli push action $d neworder '["u1", 2, "buy", "0.01000000  METH","0.01000000  METH", "100.000000 MUSDT", 2,null ]' -p $u


tcli get table $d $d queue


 tcli push action amax.mtoken transfer '["'$u'","'$d'","1.003000 MUSDT",""]' -p $u