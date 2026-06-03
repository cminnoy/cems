#!/bin/bash
pushd build
make VERBOSE=0 -j
popd
