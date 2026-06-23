#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <ostream>
#include <ratio>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace benchmark
{
namespace detail
{
template <typename type>
struct is_duration : std::false_type {};
template <typename representation, typename period>
struct is_duration<std::chrono::duration<representation, period>> : std::true_type {};

template <typename type>
concept duration = is_duration<type>::value && requires
{
  typename type::rep;
  requires std::is_arithmetic_v<typename type::rep>;
};

template <duration duration_type>
[[nodiscard]] consteval auto unit() noexcept -> std::string_view
{
  using period = typename duration_type::period;
  if constexpr (std::same_as<period, std::nano>)
    return "ns";
  else if constexpr (std::same_as<period, std::micro>)
    return "us";
  else if constexpr (std::same_as<period, std::milli>)
    return "ms";
  else if constexpr (std::same_as<period, std::ratio<1>>)
    return "s";
  else
    return "ticks";
}
}  // namespace detail

template <detail::duration duration_type>
struct record;

template <detail::duration duration_type = std::chrono::duration<double, std::milli>>
struct record
{
  using value_type = duration_type;
  using rep        = typename duration_type::rep;
  struct report_format
  {
    enum value
    {
      console,
      csv,
      json
    };
  };

  [[nodiscard]] constexpr auto mean                    () const
  {
    const auto count = std::distance(values.begin(), values.end());
    return count == 0 ? value_type {} : std::accumulate(values.begin(), values.end(), value_type {}) / static_cast<rep>(count);
  }
  [[nodiscard]] constexpr auto minimum                 () const
  {
    return values.empty() ? value_type {} : *std::min_element(values.begin(), values.end());
  }
  [[nodiscard]] constexpr auto maximum                 () const
  {
    return values.empty() ? value_type {} : *std::max_element(values.begin(), values.end());
  }
  [[nodiscard]] constexpr auto median                  () const
  {
    auto sorted = values;
    if (sorted.empty())
      return value_type {};

    std::sort(sorted.begin(), sorted.end());
    const auto middle = sorted.size() / 2;
    return sorted.size() % 2 != 0 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / static_cast<rep>(2);
  }
  [[nodiscard]] constexpr auto variance                () const
  {
    const auto count = std::distance(values.begin(), values.end());
    if (count == 0)
      return rep {};

    const auto average = mean().count();
    return std::transform_reduce(values.begin(), values.end(), rep {}, std::plus {},
                                 [average] (const value_type value) constexpr noexcept
                                 {
                                   const auto difference = value.count() - average;
                                   return difference * difference;
                                 }) / static_cast<rep>(count);
  }
  [[nodiscard]] auto           standard_deviation      () const noexcept
  {
    return value_type {static_cast<rep>(std::sqrt(variance()))};
  }
  [[nodiscard]] auto           coefficient_of_variation() const noexcept
  {
    const auto average = mean().count();
    return average == rep {} ? rep {} : standard_deviation().count() / average;
  }
  auto                         write                   (std::ostream& stream, const typename report_format::value format) const -> std::ostream&
  {
    const auto real_time = mean().count();
    if (format == report_format::console)
      return stream << name << ' ' << real_time << ' ' << values.size() << '\n';
    if (format == report_format::csv)
      return stream << name << ',' << values.size() << ',' << real_time << ',' << detail::unit<duration_type>() << '\n';

    return stream << R"({"name":")" << name
                  << R"(","iterations":)" << values.size()
                  << ",\"real_time\":" << real_time
                  << R"(,"time_unit":")" << detail::unit<duration_type>() << "\"}";
  }

  auto write_console(std::ostream& stream) const -> std::ostream&
  {
    stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n";
    return write(stream, report_format::console);
  }
  auto write_csv    (std::ostream& stream) const -> std::ostream&
  {
    stream << "name,iterations,real_time,time_unit\n";
    return write(stream, report_format::csv);
  }
  auto write_json   (std::ostream& stream) const -> std::ostream&
  {
    stream << "{\"benchmarks\":[";
    write(stream, report_format::json);
    return stream << "]}\n";
  }

  std::string                name  ;
  std::vector<duration_type> values;
};

template <detail::duration duration_type = std::chrono::duration<double, std::milli>>
class  session
{
public:
  using record_type = record<duration_type>;

  [[nodiscard]]
  auto                                      iterations() const noexcept -> std::size_t
  {
    return records_.empty() ? std::size_t {} : records_.front().values.size();
  }
  [[nodiscard]]
  auto                                      records   () const noexcept -> const std::vector<record_type>&
  {
    return records_;
  }
  auto                                      write_console(std::ostream& stream) const -> std::ostream&
  {
    stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n";
    for (const auto& record : records_)
    {
      record.write(stream, record_type::report_format::console);
    }
    return stream;
  }
  auto                                      write_csv    (std::ostream& stream) const -> std::ostream&
  {
    stream << "name,iterations,real_time,time_unit\n";
    for (const auto& record : records_)
    {
      record.write(stream, record_type::report_format::csv);
    }
    return stream;
  }
  auto                                      write_json   (std::ostream& stream) const -> std::ostream&
  {
    stream << "{\"benchmarks\":[";
    auto first = true;
    for (const auto& record : records_)
    {
      if (!first)
        stream << ',';
      first = false;
      record.write(stream, record_type::report_format::json);
    }
    return stream << "]}\n";
  }

private:
  template <detail::duration, typename>
  friend class session_recorder;

  [[nodiscard]] auto entry(const std::string_view name, const std::size_t iterations) -> record_type&
  {
    const auto entry = std::find_if(records_.begin(), records_.end(), [name] (const auto& record)
    {
      return record.name == name;
    });
    if (entry != records_.end())
      return *entry;

    records_.emplace_back(std::string {name}, std::vector<duration_type>(iterations));
    return records_.back();
  }

  std::vector<record_type> records_;
};

template <detail::duration duration_type = std::chrono::duration<double, std::milli>,
          typename clock_type = std::chrono::steady_clock>
class  session_recorder
{
public:
  session_recorder  (const std::size_t index,
                     const std::size_t iterations,
                     session<duration_type>& session) noexcept
  : index_(index), iterations_(iterations), session_(session)
  {

  }

  template <typename function_type, typename... argument_types>
  requires std::invocable<function_type&, argument_types...>
  constexpr void record(const std::string_view name, function_type&& function, argument_types&&... arguments)
  {
    const auto start = clock_type::now();
    std::invoke(function, std::forward<argument_types>(arguments)...);
    const auto end    = clock_type::now();
    auto&      result = session_.entry(name, iterations_);
    result.values[index_] = std::chrono::duration_cast<duration_type>(end - start);
  }

private:
  const std::size_t       index_     ;
  const std::size_t       iterations_;
  session<duration_type>& session_   ;
};

template <detail::duration duration_type = std::chrono::duration<double, std::milli>,
          typename clock_type = std::chrono::steady_clock,
          typename function_type>
requires std::invocable<function_type&>
[[nodiscard]] auto                    run(function_type&& function, const std::size_t iterations = 1) -> record<duration_type>
{
  auto callable = std::forward<function_type>(function);
  auto record = benchmark::record<duration_type> {"benchmark", std::vector<duration_type>(iterations)};
  for (auto i = std::size_t {}; i < iterations; ++i)
  {
    const auto start = clock_type::now();
    std::invoke(callable);
    const auto end   = clock_type::now();
    record.values[i] = std::chrono::duration_cast<duration_type>(end - start);
  }
  return record;
}
template <detail::duration duration_type = std::chrono::duration<double, std::milli>,
          typename clock_type = std::chrono::steady_clock,
          typename function_type>
requires std::invocable<function_type&, session_recorder<duration_type, clock_type>&>
[[nodiscard]] auto                    run(function_type&& function, const std::size_t iterations = 1) -> session<duration_type>
{
  auto session_callable = std::forward<function_type>(function);
  auto session = benchmark::session<duration_type> {};
  for (auto i = std::size_t {}; i < iterations; ++i)
  {
    auto recorder = session_recorder<duration_type, clock_type> {i, iterations, session};
    std::invoke(session_callable, recorder);
  }
  return session;
}

}  // namespace benchmark
