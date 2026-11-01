#!/bin/bash
# Rebuilds lib/ges/linux64/python34.a and boost_python.a (run inside the sniper SDK container,
# e.g. `podman run --rm -it -v $PWD/..:/my_mod registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest`).
# GE:S 5.0 ships the Python 3.4 standard library in gesource/python/lib, so the interpreter must be 3.4.x.
set -euo pipefail
SRC=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-/tmp/ges_py_build}
mkdir -p "$WORK" && cd "$WORK"

[ -d cpython ] || git clone --depth 1 -b v3.4.10 https://github.com/python/cpython.git cpython
[ -d boost ]   || git clone --depth 1 -b boost-1.83.0 --recurse-submodules --shallow-submodules https://github.com/boostorg/boost.git boost

# --- Python 3.4 (static, PIC, needed extension modules built in) ---
cd cpython
./configure --disable-shared --without-ensurepip OPT="-DNDEBUG -fwrapv -O2" CFLAGS="-fPIC -fno-strict-aliasing"
cp "$SRC/thirdparty/python3/Setup.local" Modules/Setup.local
make -j$(nproc) libpython3.4m.a
cp libpython3.4m.a "$SRC/lib/ges/linux64/python34.a"
cp pyconfig.h "$SRC/thirdparty/python3/linux64/pyconfig.h"
cd ..

# --- Boost.Python (static) ---
cd boost && ./bootstrap.sh && ./b2 headers && cd ..
mkdir -p bp && cd bp && rm -f *.o
for f in $(cd ../boost/libs/python/src && ls *.cpp converter/*.cpp object/*.cpp); do
    g++ -c -O2 -fPIC -std=c++11 -DBOOST_PYTHON_SOURCE -DBOOST_PYTHON_STATIC_LIB -DBOOST_PYTHON_NO_PY_SIGNATURES \
        -DBOOST_PYTHON_MAX_ARITY=20 -DNDEBUG -I../boost -I../cpython/Include -I../cpython \
        ../boost/libs/python/src/$f -o $(echo $f | tr / _ | sed 's/.cpp$/.o/')
done
ar rcs "$SRC/lib/ges/linux64/boost_python.a" *.o
strip --strip-debug "$SRC/lib/ges/linux64/"*.a
echo "done"
