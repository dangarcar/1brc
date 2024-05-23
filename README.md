# 1brc
My implementations for the 1 Billion row challenge, although they're not good enough to be submitted

## Current best time
### Custom hash map with std::forward_list, 15 threads, std::thread detached, g++ -O3
```console
real    0m2.463s
```

## Implementations
All my times had been tested with in a Ubuntu WSL2 virtual machine with 8 virtual processors and 24GB of RAM

1. std::map, single threaded, g++ -O2
```console
real    3m51.635s
```

2. std::unordered_map, single threaded, g++ -O2
```console
real    0m48.124s
```

3. std::unordered_map, reserving max station size, single threaded, g++ -O3
```console
real    0m45.070s
```

4. std::unordered_map, 15 threads, std::async, g++ -O3
```console
real    0m5.216s
```

5. Custom hash map, 15 threads, std::async, g++ -O3
```console
real    0m2.665s
```

6. Custom hash map with std::forward_list, 15 threads, std::thread detached, g++ -O3
```console
real    0m2.463s
```