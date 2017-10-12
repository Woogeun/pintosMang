#!/bin/bash
cd ./pintos/src/userprog/build
make

pintos="pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/"
sample="-p ../../tests/userprog/sample.txt -a sample.txt"
args=("args-none" "args-single" "args-multiple" "args-many" "args-dbl-space")
argv=("" "onearg" "some arguments for you!" "a b c d e f g h i j k l m n o p q r s t u v" "two  spaces!")
create=("create-normal" "create-empty" "create-long" "create-null" "create-bound" "create-exists" "create-bad-ptr")
open=("open-normal" "open-empty" "open-null" "open-boundary" "open-missing" "open-twice" "open-bad-ptr")
close=("close-normal" "close-stdin" "close-stdout" "close-twice" "close-bad-fd")
write=("write-normal" "write-stdin" "write-zero" "write-boundary" "write-bad-fd" "write-bad-ptr")
read=("read-normal" "read-stdout" "read-zero" "read-boundary" "read-bad-fd" "read-bad-ptr")


case $1 in
  "clean")
    make clean
    break;;
  "check")
    make check
    make grade
    vi grade
    break;;
  "args")
    for ((i=0;i<${#args[@]};i++)); do
      echo "$pintos${args[$i]} -a ${args[$i]} -- -q -f run '${args[$i]} ${argv[$i]}'"
    $pintos${args[$i]} -a ${args[$i]} -- -q -f run ${args[$i]} ${argv[$i]}
    done
    break;;
  "create")
    for create in "${create[@]}"; do
      echo "$pintos$create -a $create -- -q -f run $create"
      $pintos$create -a $create -- -q -f run $create
    done
    break;;
  "open")
    for open in "${open[@]}"; do
      echo "$pintos$open -a $open $sample -- -q -f run $open"
      $pintos$open -a $open $sample -- -q -f run $open
    done
    break;;
  "close")
    for close in "${close[@]}"; do
      echo "$pintos$close -a $close $sample -- -q -f run $close"
      $pintos$close -a $close $sample -- -q -f run $close
    done
    break;;
  "write")
    for write in "${write[@]}"; do
      echo "$pintos$write -a $write $sample -- -q -f run $write"
      $pintos$write -a $write $sample -- -q -f run $write
    done
    break;;
  "read")
    for read in "${read[@]}"; do
      echo "$pintos$read -a $read $sample -- -q -f run $read"
      $pintos$read -a $read $sample -- -q -f run $read
    done
    break;;
  *)
    echo "$pintos$1 -a $1 -- -q -f run $1"
    $pintos$1 -a $1 -- -q -f run $1;;
esac


