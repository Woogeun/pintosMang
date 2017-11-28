#!/bin/bash

cd pintos/src


case $1 in
  "1")
    cd threads/build
    backtrace $2
    break;;
  "2")
    cd userprog/build
    backtrace $2
    break;;
  "3")
    cd vm/build
    backtrace $2
    break;;
  "4")
    cd filesys/build
    backtrace $2
    break;;
  *)
esac


