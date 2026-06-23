#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
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

  CHECK(std::accumulate(record.values.begin(), record.values.end(), duration {}).count() == doctest::Approx(0.0));
  CHECK(record.mean                    ().count() == doctest::Approx(0.0));
  CHECK(record.median                  ().count() == doctest::Approx(0.0));
  CHECK(record.minimum                 ().count() == doctest::Approx(0.0));
  CHECK(record.maximum                 ().count() == doctest::Approx(0.0));
  CHECK(record.variance                ()         == doctest::Approx(0.0));
  CHECK(record.standard_deviation      ().count() == doctest::Approx(0.0));
  CHECK(record.coefficient_of_variation()         == doctest::Approx(0.0));
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

  CHECK(std::accumulate(record.values.begin(), record.values.end(), duration {}).count() == doctest::Approx(expected_sum));
  CHECK(record.mean                    ().count() == doctest::Approx(expected_average));
  CHECK(record.median                  ().count() == doctest::Approx(expected_average));
  CHECK(record.minimum                 ().count() == doctest::Approx(expected_minimum));
  CHECK(record.maximum                 ().count() == doctest::Approx(expected_maximum));
  CHECK(record.variance                ()         == doctest::Approx(expected_variance));
  CHECK(record.standard_deviation      ().count() == doctest::Approx(std::sqrt(expected_variance)));
  CHECK(record.coefficient_of_variation()         == doctest::Approx(std::sqrt(expected_variance) / expected_average));
}

TEST_CASE("benchmark::record supports integral chrono durations")
{
  using integral_duration = std::chrono::milliseconds;
  const auto record = benchmark::record<integral_duration> {
    "fixed",
    {integral_duration {1}, integral_duration {2}, integral_duration {3}}
  };

  CHECK(record.mean              ().count() == 2);
  CHECK(record.median            ().count() == 2);
  CHECK(record.minimum           ().count() == 1);
  CHECK(record.maximum           ().count() == 3);
  CHECK(record.standard_deviation().count() == 0);
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
  const auto minimum = record.minimum();
  CHECK(minimum                        >= duration {});
  CHECK(record.maximum() >= minimum);
  CHECK(record.mean   () >= duration {});
}

TEST_CASE("benchmark::run defaults to milliseconds")
{
  const auto record = benchmark::run([] {});
  static_assert(std::same_as<typename decltype(record)::value_type, std::chrono::duration<double, std::milli>>);

  auto stream = std::ostringstream {};
  record.write_console(stream);
  CHECK(stream.str().contains("Benchmark Time(ms) Iterations"));
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

TEST_CASE("benchmark::session_recorder forwards callable arguments")
{
  auto value = std::size_t {};

  const auto session = benchmark::run<duration>([&value] (auto& recorder)
  {
    recorder.record("add", [] (auto& target, const auto amount)
    {
      target += amount;
    }, value, std::size_t {2});
  }, 3);

  CHECK(value == 6);
  REQUIRE(session.records   ().size() == 1);
  CHECK  (session.records   ()[0].name == "add");
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
  record.write_console(stream);
  CHECK(stream.str().contains("Benchmark Time(ms) Iterations"));
  CHECK(stream.str().contains("fixed 1.5 2"));

  stream.str({});
  record.write_csv(stream);
  CHECK(stream.str() == "name,iterations,real_time,time_unit\nfixed,2,1.5,ms\n");

  stream.str({});
  record.write(stream, decltype(record)::report_format::csv);
  CHECK(stream.str() == "fixed,2,1.5,ms\n");

  stream.str({});
  record.write_json(stream);
  CHECK(stream.str() == "{\"benchmarks\":[{\"name\":\"fixed\",\"iterations\":2,\"real_time\":1.5,"
                        "\"time_unit\":\"ms\"}]}\n");

  stream.str({});
  session.write_console(stream);
  CHECK(stream.str().contains("alpha"));

  stream.str({});
  session.write_csv(stream);
  CHECK(stream.str().contains("beta,2,"));

  stream.str({});
  session.write_json(stream);
  CHECK(stream.str().contains("\"name\":\"alpha\""));
}
