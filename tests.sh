#!/bin/bash

function exec_test () {
./poet_main $1 > /dev/null << EOF
-1
900
100
50
5
0
1
10
EOF

# cat $1 | tail -n 105 > $1
#cat $1 | tail -n 105 > $1.tmp
#cp $1.tmp $1
#rm $1.tmp
}

export -f exec_test

# exec_test "out.txt"

SHELL=/bin/bash parallel -j8 exec_test ::: out{0..9}.txt
