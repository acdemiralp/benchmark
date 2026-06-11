#include <algorithm>
#include <cstddef>
#include <vector>

#include <doctest/doctest.h>

#include <benchmark/benchmark.hpp>

TEST_CASE("bm::run")
{
  std::vector<std::size_t> buffer(100000);

  const auto record = bm::run<float, std::milli>([&]
  {
    std::iota(buffer.begin(), buffer.end(), 0);
  }, 10 /* iterations */);
  auto mean               = record.mean              ();
  auto variance           = record.variance          ();
  auto standard_deviation = record.standard_deviation();
  record.to_csv("output_single.csv");

  const auto session = bm::run<float, std::milli>([&buffer] (auto& recorder)
  {
    recorder.record("iota"    , [&buffer]
    {
      std::iota(buffer.begin(), buffer.end(), 0);
    });
    recorder.record("generate", [&buffer]
    {
      std::generate(buffer.begin(), buffer.end(), std::rand);
    });
  }, 10 /* iterations */);
  for (const auto& record : session.records)
  {
    auto name               = record.name;
    auto mean               = record.mean              ();
    auto variance           = record.variance          ();
    auto standard_deviation = record.standard_deviation();
  }
  session.to_csv("output_multi.csv");
}
