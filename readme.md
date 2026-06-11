# acdemiralp/benchmark
Single-header C++23 benchmarking library.

## Building
```
./bootstrap.[bat|sh]
cmake --preset ninja-multi
cmake --build --preset release
```

## Using
```cmake
find_package         (benchmark CONFIG REQUIRED)
target_link_libraries(application PRIVATE benchmark::benchmark)
```

## Example
TODO