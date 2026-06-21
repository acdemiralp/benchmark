#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <sstream>
#include <vector>

#include <doctest/doctest.h>

#include <benchmark/benchmark.hpp>

using duration = std::chrono::duration<double, std::milli>;

TEST_CASE("benchmark::record statistics are empty-safe")
{
  const auto record = benchmark::record<duration> {};

  CHECK(record.sum                     ().count() == doctest::Approx(0.0));
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
  const auto record = benchmark::record<duration> {"fixed", {duration {1.0}, duration {2.0}, duration {3.0}, duration {4.0}}};

  CHECK(record.sum                     ().count() == doctest::Approx(10.0));
  CHECK(record.mean                    ().count() == doctest::Approx( 2.5));
  CHECK(record.median                  ().count() == doctest::Approx( 2.5));
  CHECK(record.minimum                 ().count() == doctest::Approx( 1.0));
  CHECK(record.maximum                 ().count() == doctest::Approx( 4.0));
  CHECK(record.variance                ()         == doctest::Approx(1.25));
  CHECK(record.standard_deviation      ().count() == doctest::Approx(std::sqrt(1.25)));
  CHECK(record.coefficient_of_variation()         == doctest::Approx(std::sqrt(1.25) / 2.5));
}

TEST_CASE("benchmark::run records a single callable")
{
  auto counter = std::size_t {};

  const auto record = benchmark::run<duration>([&]
  {
    ++counter;
  }, 10 /* iterations */);

  CHECK(record.name          == "benchmark");
  CHECK(record.values.size() == 10);
  CHECK(counter              == 10);
  CHECK(record.minimum       () >= duration {});
  CHECK(record.maximum       () >= record.minimum());
  CHECK(record.mean          () >= duration {});
}

TEST_CASE("benchmark::run records named session entries")
{
  auto buffer = std::vector<std::size_t>(1000);

  const auto session = benchmark::run<duration>([&buffer] (auto& recorder)
  {
    recorder.record("iota"    , [&buffer]
    {
      std::iota(buffer.begin(), buffer.end(), 0);
    });
    recorder.record("generate", [&buffer]
    {
      auto value = std::size_t {};
      std::ranges::generate(buffer, [&value] { return value++; });
    });
  }, 10 /* iterations */);

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
  }, 3 /* iterations */);

  REQUIRE(session.records   ().size() == 1);
  CHECK  (session.iterations   () == 3);
  CHECK  (session.records()[0].name          == "same");
  CHECK  (session.records()[0].values.size() == 3);
}

TEST_CASE("benchmark reporters produce console csv and json output")
{
  const auto record = benchmark::record<duration> {"fixed", {duration {1.0}, duration {2.0}}};
  const auto session = benchmark::run<duration>([] (auto& recorder)
  {
    recorder.record("alpha", [] {});
    recorder.record("beta" , [] {});
  }, 2 /* iterations */);

  auto stream = std::ostringstream {};
  benchmark::write_console(stream, record);
  CHECK(stream.str().contains("Benchmark Time(ms) Iterations"));
  CHECK(stream.str().contains("fixed 1.5 2"));

  stream.str({});
  benchmark::write_csv(stream, record);
  CHECK(stream.str() == "name,iterations,real_time,time_unit\n\"fixed\",2,1.5,ms\n");

  stream.str({});
  benchmark::write_json(stream, record);
  CHECK(stream.str() == "{\"benchmarks\":[{\"name\":\"fixed\",\"iterations\":2,\"real_time\":1.5,\"time_unit\":\"ms\"}]}\n");

  stream.str({});
  benchmark::write_console(stream, session);
  CHECK(stream.str().contains("alpha"));

  stream.str({});
  benchmark::write_csv(stream, session);
  CHECK(stream.str().contains("\"beta\",2,"));

  stream.str({});
  benchmark::write_json(stream, session);
  CHECK(stream.str().contains("\"name\":\"alpha\""));
}

TEST_CASE("benchmark reporters quote names")
{
  const auto record = benchmark::record<duration> {"quote\"slash\\", {duration {1.0}}};
  auto stream = std::ostringstream {};

  benchmark::write_csv(stream, record);
  CHECK(stream.str() == "name,iterations,real_time,time_unit\n\"quote\"\"slash\\\",1,1,ms\n");

  stream.str({});
  benchmark::write_json(stream, record);
  CHECK(stream.str() == "{\"benchmarks\":[{\"name\":\"quote\\\"slash\\\\\",\"iterations\":1,\"real_time\":1,\"time_unit\":\"ms\"}]}\n");
}
