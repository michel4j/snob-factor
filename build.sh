#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -d build ]; then
    /bin/rm -rf build/
fi

mkdir build    &&
cd build       &&
cmake ..       &&
cmake --build . --config Release &&
if [ -f Release/_snob.so ]; then
    /bin/cp Release/_snob.so ../snob/
else
    /bin/cp _snob.so ../snob/
fi

cd $SCRIPT_DIR
