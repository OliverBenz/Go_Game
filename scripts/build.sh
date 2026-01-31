#!/bin/bash

. ./settings.sh

echo "Entering Development Environment"
echo "Using configuration: $configuration"
nix develop .. --command bash -c "
  cmake -S .. -B ../build -DCMAKE_BUILD_TYPE=$configuration -DBUILD_TESTS=True -DBUILD_EXAMPLES=True
  cmake --build ../build -j
"