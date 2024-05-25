#!/bin/bash

echo "Running..."

g++ --std=c++23 -O3 -msse3 solution.cpp -o solution

time ./solution