#!/bin/bash
cd pintos/src/filesys
make
cd build

case $1 in
  "clean")
    cd ../
    make clean
    break;;
  "check")
    make grade
    vi grade
    break;;
  "dir")

    break;;
  "grow")

    break;;
  *)
    break;;

  esac

