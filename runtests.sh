#! /bin/sh

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

function runtest {
    local fname=$(basename "${1%.*}")

    cat $1 | xargs ./booltab > $fname.tmp

    if diff $fname.tmp tests/$fname.out
    then
        printf "${GREEN}test $1 PASSED${NC}\n"
    else
        printf "${RED}test $1 FAILED${NC}\n"
    fi

    rm $fname.tmp
}


for file in ./tests/*.in; do
    runtest $file
done