#!/bin/bash

for p in 0.1 0.2 0.3 0.4 0.5; do
    cp -r ../input.origin/all ../input.origin/p${p}train
    cp -r ../input.origin/all ../input.origin/p${p}test
done
