#!/bin/bash

function exec_test () {
./poet_main $1 > /dev/null << EOF
-1
90
10
1000
5
0
1
30
EOF

# cat $1 | tail -n 105 > $1
#cat $1 | tail -n 105 > $1.tmp
#cp $1.tmp $1
#rm $1.tmp
}

export -f exec_test

# exec_test "out.txt"

SHELL=/bin/bash parallel -j32 exec_test ::: out{0..63}.txt
