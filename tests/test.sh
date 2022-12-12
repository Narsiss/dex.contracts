d=dexdexdexde2
a=dexadmin
tnew $d

tset $d orderbookdex


tcli push action $d init '[]' -p $d

tcli get table $d $d config

tcli get table $d $d sympair


tcli push action $d setsympair '[["6,MUSDT", "amax.mtoken"],["8,METH", "amax.mtoken"],"1.000000 MUSDT","0.010000 MUSDT", true, false ]' -p $a