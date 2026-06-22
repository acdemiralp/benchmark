# acdemiralp/benchmark
Single-header C++23 benchmarking library with no required `main` replacement.

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
```cpp
#include <algorithm>
#include <chrono>
#include <iostream>
#include <print>
#include <vector>

#include <benchmark/benchmark.hpp>

int main()
{
  auto values = std::vector<int>(1000);

  using milliseconds = std::chrono::duration<double, std::milli>;

  const auto record = benchmark::run<milliseconds>([&]
  {
    std::ranges::sort(values);
  }, 100);

  std::println("mean: {} ms", benchmark::mean(record.values.begin(), record.values.end()).count());
  std::println("standard deviation: {} ms", benchmark::standard_deviation(record.values.begin(), record.values.end()).count());

  const auto session = benchmark::run<milliseconds>([&] (auto& recorder)
  {
    recorder.record("sort", [&]
    {
      std::ranges::sort(values);
    });
  }, 100);

  benchmark::write_console(std::cout, session);
}
```

Reports are available as compact stream output:

```cpp
benchmark::write_console(std::cout, session);
benchmark::write_csv    (std::cout, session);
benchmark::write_json   (std::cout, session);
