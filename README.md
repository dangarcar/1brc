# 1brc
My implementations for the 1 Billion row challenge, although they're not good enough to be submitted

## Implementations
All my times had been tested with in a Ubuntu WSL2 virtual machine with 8 virtual processors and 24GB of RAM

- 1 std::map, single threaded
```console
real    3m51.635s
user    3m51.140s
sys     0m0.439s
```

- 2 std::unordered_map, single threaded
```console
real    0m48.124s
user    0m47.465s
sys     0m0.582s
```