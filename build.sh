#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


if [ -d build ]; then
    /bin/rm -rf build/
fi

mkdir build    &&
cd build       &&
cmake ..       &&
make           &&
/bin/cp _snob.so ../snob/

cd $SCRIPT_DIR
