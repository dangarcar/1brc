#!/bin/bash

echo "Running..."

time { g++-14 --std=c++23 -O2 solution.cpp -o solution && ./solution; }