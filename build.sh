#!/bin/bash

LIBS="-lm"
INCS=""
DEFS=""

if [[ $OS = *"Windows"* ]]; then
  LIBS="$LIBS -largp"
  DEFS="-D_GNU_SOURCE"
elif [[ `uname` = Darwin ]]; then
  if [ -f /opt/local/include/argp.h ]; then
    # MacPorts packages required: argp-standalone
    LIBS="$LIBS -L/opt/local/lib -largp"
    INCS="$INCS -I/opt/local/include"

    # (MacPorts md5sha1sum required for tests)
  else
    echo "argp not found - but will try building anyway"
  fi
else
  DEFS="-D_GNU_SOURCE"
fi

gcc -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -O3 $DEFS $INCS -o decode6809 src/main.c src/memory.c src/em_6809.c src/dis_6809.c $LIBS
