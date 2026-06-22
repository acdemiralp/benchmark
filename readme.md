# acdemiralp/benchmark
Single-header C++23 benchmarking library.

## Building
```bash
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
#include <benchmark/benchmark.hpp>

std::int32_t main(std::int32_t argc, char** argv)
{
  auto values = std::vector<std::int32_t>(1000);

  const auto record  = benchmark::run<std::chrono::duration<double, std::milli>>([&]
  {
    std::ranges::sort(values);
  }, 100);

  std::print     (record);
  record .to_csv ("benchmark.csv" );
  record .to_json("benchmark.json");

  const auto session = benchmark::run<std::chrono::duration<double, std::milli>>([&] (auto& recorder)
  {
    recorder.record("sort", [&]
    {
      std::ranges::sort(values);
    });
  }, 100);

  std::print     (session);
  session.to_csv ("benchmark.csv" );
  session.to_json("benchmark.json");

  return 0;
}
```
