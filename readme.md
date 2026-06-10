# acdemiralp/benchmark
Single-header benchmarking library.

### Building
```
./bootstrap.[bat|sh]
cmake --preset ninja-multi
cmake --build --preset release
```

## Using
```cpp
#include <algorithm>
#include <cstddef>
#include <vector>

#include <benchmark/benchmark.hpp>

int main()
{
  std::vector<std::size_t> buffer(100000);

  const auto record = bm::run<float, std::milli>([&]
  {
    std::iota(buffer.begin(), buffer.end(), std::size_t{0});
  }, 100);
  record.to_csv("output_single.csv");

  const auto session = bm::run<float, std::milli>([&buffer] (auto& recorder)
  {
    recorder.record("iota", [&buffer]
    {
      std::iota(buffer.begin(), buffer.end(), std::size_t{0});
    });
    recorder.record("generate", [&buffer]
    {
      std::generate(buffer.begin(), buffer.end(), std::rand);
    });
  }, 100);
  session.to_csv("output_multi.csv");
}
```