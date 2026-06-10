# acdemiralp/benchmark
Single-header benchmarking library.

### Bootstrap
```powershell
./bootstrap.bat
```

```powershell
cmake --preset ninja-multi
cmake --build --preset debug
cmake --preset ninja-multi-tests
cmake --build --preset debug-tests
ctest --preset debug
```

## API

### `bm::record<type>`
Stores benchmark samples and computes mean, variance, and standard deviation. Records can also be exported as CSV.

### `bm::session<type>`
Stores a set of named records and can export the whole session as CSV.

### `bm::session_recorder<type, period>`
Records named benchmark runs into a session during each iteration.

### `bm::run<type, period>`
Runs either a single benchmark function or a session recorder function over a fixed number of iterations.

## Example
```cpp
#include <algorithm>
#include <cstddef>
#include <vector>

#include <bm/bm.hpp>

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