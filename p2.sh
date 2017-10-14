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
  
  "exec")
    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-once -a exec-once -p tests/userprog/child-simple -a child-simple -- -q   -f run exec-once"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-once -a exec-once -p tests/userprog/child-simple -a child-simple -- -q   -f run exec-once

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-arg -a exec-arg -p tests/userprog/child-args -a child-args -- -q   -f run exec-arg"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-arg -a exec-arg -p tests/userprog/child-args -a child-args -- -q   -f run exec-arg

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-multiple -a exec-multiple -p tests/userprog/child-simple -a child-simple -- -q   -f run exec-multiple"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-multiple -a exec-multiple -p tests/userprog/child-simple -a child-simple -- -q   -f run exec-multiple

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-missing -a exec-missing -- -q   -f run exec-missing"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-missing -a exec-missing -- -q   -f run exec-missing

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-bad-ptr -a exec-bad-ptr -- -q   -f run exec-bad-ptr"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/exec-bad-ptr -a exec-bad-ptr -- -q   -f run exec-bad-ptr
    break;;
  
  "wait")
    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-simple -a wait-simple -p tests/userprog/child-simple -a child-simple -- -q   -f run wait-simple"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-simple -a wait-simple -p tests/userprog/child-simple -a child-simple -- -q   -f run wait-simple

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-twice -a wait-twice -p tests/userprog/child-simple -a child-simple -- -q   -f run wait-twice"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-twice -a wait-twice -p tests/userprog/child-simple -a child-simple -- -q   -f run wait-twice

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-killed -a wait-killed -p tests/userprog/child-bad -a child-bad -- -q   -f run wait-killed"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-killed -a wait-killed -p tests/userprog/child-bad -a child-bad -- -q   -f run wait-killed

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-bad-pid -a wait-bad-pid -- -q   -f run wait-bad-pid"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/wait-bad-pid -a wait-bad-pid -- -q   -f run wait-bad-pid

    echo "pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/multi-recurse -a multi-recurse -- -q   -f run 'multi-recurse 15'"
    pintos -v -k -T 3 --qemu  --fs-disk=2 -p tests/userprog/multi-recurse -a multi-recurse -- -q   -f run 'multi-recurse 15'
    break;;
  
  "sc")
    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-bad-arg -a sc-bad-arg -- -q -f run sc-bad-arg"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-bad-arg -a sc-bad-arg -- -q -f run sc-bad-arg

    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-bad-sp -a sc-bad-sp -- -q -f run sc-bad-sp"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-bad-sp -a sc-bad-sp -- -q -f run sc-bad-sp

    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-boundary -a sc-boundary -- -q -f run sc-boundary"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-boundary -a sc-boundary -- -q -f run sc-boundary

    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-boundary-2 -a sc-boundary-2 -- -q -f run sc-boundary-2"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/sc-boundary-2 -a sc-boundary-2 -- -q -f run sc-boundary-2
    break;;

  "rox")
    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/rox-simple -a rox-simple -- -q -f run rox-simple"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/rox-simple -a rox-simple -- -q -f run rox-simple

    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/rox-child -a rox-child -p ./tests/userprog/child-rox -a child-rox -- -q -f run rox-child"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/rox-child -a rox-child -p ./tests/userprog/child-rox -a child-rox -- -q -f run rox-child

    echo "pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/rox-multichild -a rox-multichild -p ./tests/userprog/child-rox -a child-rox -- -q -f run rox-multichild"
    pintos -v -k -T 3 --qemu --fs-disk=2 -p tests/userprog/rox-multichild -a rox-multichild -p ./tests/userprog/child-rox -a child-rox -- -q -f run rox-multichild
    break;;
  *)
    echo "$pintos$1 -a $1 -- -q -f run $1"
    $pintos$1 -a $1 -- -q -f run $1;;
esac


