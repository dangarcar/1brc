#!/bin/bash

echo "Running..."

time { g++ -std=c++2a -O2 solution.cpp -o solution && ./solution; }