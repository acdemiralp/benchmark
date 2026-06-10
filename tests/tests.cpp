#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

#include <doctest/doctest.h>

#include <bm/bm.hpp>

TEST_CASE("bm::record computes summary statistics")
{
  const bm::record<double> record{"sample", {1.0, 2.0, 3.0}};

  CHECK(record.mean() == doctest::Approx(2.0));
  CHECK(record.variance() == doctest::Approx(2.0 / 3.0));
  CHECK(record.standard_deviation() == doctest::Approx(std::sqrt(2.0 / 3.0)));
}

TEST_CASE("bm::run records the requested iteration count")
{
  std::vector<std::size_t> buffer(1024);

  const auto record = bm::run<float, std::milli>([&buffer]
  {
    std::iota(buffer.begin(), buffer.end(), std::size_t{0});
  }, 4);

  CHECK(record.name == "benchmark");
  CHECK(record.values.size() == 4);
}

TEST_CASE("bm::run session groups named benchmarks across iterations")
{
  std::vector<std::size_t> buffer(256);

  const auto session = bm::run<float, std::milli>([&buffer] (auto& recorder)
  {
    recorder.record("iota", [&buffer]
    {
      std::iota(buffer.begin(), buffer.end(), std::size_t{0});
    });
    recorder.record("generate", [&buffer]
    {
      std::generate(buffer.begin(), buffer.end(), [value = std::size_t{0}] () mutable
      {
        return value++;
      });
    });
  }, 3);

  REQUIRE(session.records.size() == 2);
  CHECK(session.records[0].values.size() == 3);
  CHECK(session.records[1].values.size() == 3);
}