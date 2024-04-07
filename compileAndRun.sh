#!/bin/bash

g++ -Wall -Wextra -pedantic `pkgconf --cflags atspi-2` `pkgconf --libs gobject-2.0 atspi-2` main.cpp -o AtspiTextTest

if [[ $? -ne 0 ]]; then
  echo "Error during compilation!"
else
  ./AtspiTextTest
fi

read -n1 -rs -p $'Press any key to continue...\n'
