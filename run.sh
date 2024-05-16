#!/bin/bash

echo "Running..."

time { g++ --std=c++23 -O3 solution.cpp -o solution && ./solution; }