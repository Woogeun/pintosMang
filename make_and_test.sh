#!/bin/bash
cd ./pintos/src/threads
make


pintos="../utils/pintos -v -- -q run"
alarm=("alarm-single" "alarm-multiple" "alarm-zero" "alarm-negative" "alarm-simultaneous" "alarm-priority")


case $1 in
  "hello")
    $pintos hello
    break;;
  "alarm")
    for alarm in "${alarm[@]}"; do
      echo "$pintos $alarm"
      $pintos $alarm
    done
    break;;
  *)
    echo "$pintos $1"
    $pintos $1;;
    
esac
