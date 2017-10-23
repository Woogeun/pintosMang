#!/bin/bash
cd pintos/src/
make clean
cd ../
tar -czvf 36.tar.gz src
mv 36.tar.gz ../
