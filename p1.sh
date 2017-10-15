#!/bin/bash
cd ./pintos/src/threads
make


pintos="../utils/pintos -v -- -q run"
pintos_test="make /test/threads/"
alarm=("alarm-single" "alarm-multiple" "alarm-zero" "alarm-negative" "alarm-simultaneous" "alarm-priority")

priority=("priority-change" "priority-donate-one" "priority-donate-multiple" "priority-donate-multiple2" "priority-donate-nest" "priority-donate-sema" "priority-donate-lower" "priority-donate-chain" "priority-fifo" "priority-preempt" "priority-sema" "priority-condvar")

case $1 in
  "clean")
    make clean
    break;;
  "check")
    cd ./build
    make check
    make grade
    vi grade
    break;;
  "alarm")
    for alarm in "${alarm[@]}"; do
      echo "$pintos $alarm"
      $pintos $alarm
    done
    break;;
  "priority")
    for priority in "${priority[@]}"; do
      echo "$pintos $priority"
      $pintos $priority
    done
    break;;
  *)
    echo "$pintos $1"
    $pintos $1;;
    
esac
