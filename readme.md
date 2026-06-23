# acdemiralp/benchmark
Header-only C++23 benchmarking library.

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

  record.write_console(std::cout);
  record.write_csv    (std::cout);
  record.write_json   (std::cout);

  const auto session = benchmark::run<std::chrono::duration<double, std::milli>>([&] (auto& recorder)
  {
    recorder.record("sort", [&]
    {
      std::ranges::sort(values);
    });
  }, 100);

  session.write_console(std::cout);
  session.write_csv    (std::cout);
  session.write_json   (std::cout);

  return 0;
}
```
