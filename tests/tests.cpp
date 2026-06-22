#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <ratio>
#include <sstream>
#include <vector>

#include <doctest/doctest.h>

#include <benchmark/benchmark.hpp>

using duration = std::chrono::duration<double, std::milli>;

TEST_CASE("benchmark::record statistics are empty-safe")
{
  const auto record = benchmark::record<duration> {};
  const auto first  = record.values.begin();
  const auto last   = record.values.end  ();

  CHECK(std::accumulate(first, last, duration {}).count() == doctest::Approx(0.0));
  CHECK(benchmark::mean                    (first, last).count() == doctest::Approx(0.0));
  CHECK(benchmark::median                  (first, last).count() == doctest::Approx(0.0));
  CHECK(benchmark::minimum                 (first, last).count() == doctest::Approx(0.0));
  CHECK(benchmark::maximum                 (first, last).count() == doctest::Approx(0.0));
  CHECK(benchmark::variance                (first, last)         == doctest::Approx(0.0));
  CHECK(benchmark::standard_deviation      (first, last).count() == doctest::Approx(0.0));
  CHECK(benchmark::coefficient_of_variation(first, last)         == doctest::Approx(0.0));
}

TEST_CASE("benchmark::record statistics use chrono durations")
{
  constexpr auto expected_sum      = 10.0;
  constexpr auto expected_average  =  2.5;
  constexpr auto expected_minimum  =  1.0;
  constexpr auto expected_maximum  =  4.0;
  constexpr auto expected_variance =  1.25;
  const auto record = benchmark::record<duration> {
    "fixed",
    {duration {1.0}, duration {2.0}, duration {3.0}, duration {4.0}}
  };
  const auto first  = record.values.begin();
  const auto last   = record.values.end  ();

  CHECK(std::accumulate(first, last, duration {}).count() == doctest::Approx(expected_sum));
  CHECK(benchmark::mean                    (first, last).count() == doctest::Approx(expected_average));
  CHECK(benchmark::median                  (first, last).count() == doctest::Approx(expected_average));
  CHECK(benchmark::minimum                 (first, last).count() == doctest::Approx(expected_minimum));
  CHECK(benchmark::maximum                 (first, last).count() == doctest::Approx(expected_maximum));
  CHECK(benchmark::variance                (first, last)         == doctest::Approx(expected_variance));
  CHECK(benchmark::standard_deviation      (first, last).count() == doctest::Approx(std::sqrt(expected_variance)));
  CHECK(benchmark::coefficient_of_variation(first, last)         == doctest::Approx(std::sqrt(expected_variance) / expected_average));
}

TEST_CASE("benchmark::run records a single callable") // NOLINT(readability-function-cognitive-complexity)
{
  auto counter = std::size_t {};

  const auto record = benchmark::run<duration>([&]
  {
    ++counter;
  }, 10);

  CHECK(record.name          == "benchmark");
  CHECK(record.values.size() == 10);
  CHECK(counter              == 10);
  const auto first   = record.values.begin();
  const auto last    = record.values.end  ();
  const auto minimum = benchmark::minimum(first, last);
  CHECK(minimum                        >= duration {});
  CHECK(benchmark::maximum(first, last) >= minimum);
  CHECK(benchmark::mean   (first, last) >= duration {});
}

TEST_CASE("benchmark::run records named session entries") // NOLINT(readability-function-cognitive-complexity)
{
  constexpr auto buffer_size = std::size_t {1000};
  auto buffer = std::vector<std::size_t>(buffer_size);

  const auto session = benchmark::run<duration>([&buffer] (auto& recorder)
  {
    recorder.record("iota"    , [&buffer]
    {
      std::iota(buffer.begin(), buffer.end(), 0);
    });
    recorder.record("generate", [&buffer]
    {
      auto value = std::size_t {};
      std::generate(buffer.begin(), buffer.end(), [&value] { return value++; });
    });
  }, 10);

  REQUIRE(session.records   ().size() == 2);
  CHECK  (session.iterations   () == 10);
  CHECK  (session.records()[0].name          == "iota");
  CHECK  (session.records()[0].values.size() == 10);
  CHECK  (session.records()[1].name          == "generate");
  CHECK  (session.records()[1].values.size() == 10);
}

TEST_CASE("benchmark::run reuses session records by name")
{
  const auto session = benchmark::run<duration>([] (auto& recorder)
  {
    recorder.record("same", [] {});
    recorder.record("same", [] {});
  }, 3);

  REQUIRE(session.records   ().size() == 1);
  CHECK  (session.iterations() == 3);
  CHECK  (session.records   ()[0].name          == "same");
  CHECK  (session.records   ()[0].values.size() == 3     );
}

TEST_CASE("benchmark reporters produce console csv and json output") // NOLINT(readability-function-cognitive-complexity)
{
  const auto record  = benchmark::record<duration> {"fixed", {duration {1.0}, duration {2.0}}};
  const auto session = benchmark::run<duration>([] (auto& recorder)
  {
    recorder.record("alpha", [] {});
    recorder.record("beta" , [] {});
  }, 2);

  auto stream = std::ostringstream {};
  benchmark::write_console(stream, record);
  CHECK(stream.str().contains("Benchmark Time(ms) Iterations"));
  CHECK(stream.str().contains("fixed 1.5 2"));

  stream.str({});
  benchmark::write_csv(stream, record);
  CHECK(stream.str() == "name,iterations,real_time,time_unit\nfixed,2,1.5,ms\n");

  stream.str({});
  benchmark::write_json(stream, record);
  CHECK(stream.str() == "{\"benchmarks\":[{\"name\":\"fixed\",\"iterations\":2,\"real_time\":1.5,"
                        "\"time_unit\":\"ms\"}]}\n");

  stream.str({});
  benchmark::write_console(stream, session);
  CHECK(stream.str().contains("alpha"));

  stream.str({});
  benchmark::write_csv(stream, session);
  CHECK(stream.str().contains("beta,2,"));

  stream.str({});
  benchmark::write_json(stream, session);
  CHECK(stream.str().contains("\"name\":\"alpha\""));
}
