#!/bin/bash

cd pintos/src/vm
make
cd build

case $1 in
  "clean")
    cd ../
    make clean
    break;;
  "pt")
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-grow-stack -a pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-grow-pusha -a pt-grow-pusha --swap-disk=4 -- -q   -f run pt-grow-pusha
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-grow-bad -a pt-grow-bad --swap-disk=4 -- -q   -f run pt-grow-bad
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-big-stk-obj -a pt-big-stk-obj --swap-disk=4 -- -q   -f run pt-big-stk-obj
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-bad-addr -a pt-bad-addr --swap-disk=4 -- -q   -f run pt-bad-addr
    * pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-bad-read -a pt-bad-read -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run pt-bad-read 
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-write-code -a pt-write-code --swap-disk=4 -- -q   -f run pt-write-code
    * pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-write-code2 -a pt-write-code2 -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run pt-write-code2
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/pt-grow-stk-sc -a pt-grow-stk-sc --swap-disk=4 -- -q   -f run pt-grow-stk-sc
    break;;
  "page")
    pintos -v -k -T 300 --bochs  --fs-disk=2 -p tests/vm/page-linear -a page-linear --swap-disk=4 -- -q   -f run page-linear
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/page-parallel -a page-parallel -p tests/vm/child-linear -a child-linear --swap-disk=4 -- -q   -f run page-parallel
    pintos -v -k -T 600 --bochs  --fs-disk=2 -p tests/vm/page-merge-seq -a page-merge-seq -p tests/vm/child-sort -a child-sort --swap-disk=4 -- -q   -f run page-merge-seq
    pintos -v -k -T 600 --bochs  --fs-disk=2 -p tests/vm/page-merge-par -a page-merge-par -p tests/vm/child-sort -a child-sort --swap-disk=4 -- -q   -f run page-merge-par
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/page-merge-stk -a page-merge-stk -p tests/vm/child-qsort -a child-qsort --swap-disk=4 -- -q   -f run page-merge-stk
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/page-merge-mm -a page-merge-mm -p tests/vm/child-qsort-mm -a child-qsort-mm --swap-disk=4 -- -q   -f run page-merge-mm
    pintos -v -k -T 600 --bochs  --fs-disk=2 -p tests/vm/page-shuffle -a page-shuffle --swap-disk=4 -- -q   -f run page-shuffle
    break;;
  "mmap")
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-read -a mmap-read -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-read
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-close -a mmap-close -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-close
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-unmap -a mmap-unmap -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-unmap
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-overlap -a mmap-overlap -p tests/vm/zeros -a zeros --swap-disk=4 -- -q   -f run mmap-overlap
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-twice -a mmap-twice -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-twice
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-write -a mmap-write --swap-disk=4 -- -q   -f run mmap-write
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-exit -a mmap-exit -p tests/vm/child-mm-wrt -a child-mm-wrt --swap-disk=4 -- -q   -f run mmap-exit
    pintos -v -k -T 600 --bochs  --fs-disk=2 -p tests/vm/mmap-shuffle -a mmap-shuffle --swap-disk=4 -- -q   -f run mmap-shuffle
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-bad-fd -a mmap-bad-fd --swap-disk=4 -- -q   -f run mmap-bad-fd
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-clean -a mmap-clean -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-clean
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-inherit -a mmap-inherit -p ../../tests/vm/sample.txt -a sample.txt -p tests/vm/child-inherit -a child-inherit --swap-disk=4 -- -q   -f run mmap-inherit
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-misalign -a mmap-misalign -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-misalign
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-null -a mmap-null -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-nul
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-over-code -a mmap-over-code -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-over-code
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-over-data -a mmap-over-data -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-over-data
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-over-stk -a mmap-over-stk -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-over-stk
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-remove -a mmap-remove -p ../../tests/vm/sample.txt -a sample.txt --swap-disk=4 -- -q   -f run mmap-remove
    pintos -v -k -T 60 --bochs  --fs-disk=2 -p tests/vm/mmap-zero -a mmap-zero --swap-disk=4 -- -q   -f run mmap-zero
    break;;
   *)
     break;;
 esac

