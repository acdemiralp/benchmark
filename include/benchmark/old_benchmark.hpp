#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <ctime>
#include <cstddef>
#include <functional>
#include <numeric>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace benchmark
{
template <detail::duration duration_type = std::chrono::duration<double, std::milli>>
struct record
{
  using value_type = duration_type;
  using rep        = typename duration_type::rep;

  [[nodiscard]] constexpr auto mean                    () const
  {
    const auto count = values.size();
    return count == 0 ? value_type {} : std::accumulate(values.begin(), values.end(), value_type {}) / static_cast<rep>(count);
  }
  [[nodiscard]] constexpr auto cpu_mean                () const
  {
    const auto count = cpu_values.size();
    return count == 0 ? value_type {} : std::accumulate(cpu_values.begin(), cpu_values.end(), value_type {}) / static_cast<rep>(count);
  }
  [[nodiscard]] constexpr auto minimum                 () const
  {
    return values.empty() ? value_type {} : *std::ranges::min_element(values);
  }
  [[nodiscard]] constexpr auto maximum                 () const
  {
    return values.empty() ? value_type {} : *std::ranges::max_element(values);
  }
  [[nodiscard]] constexpr auto median                  () const
  {
    auto sorted = values;
    if (sorted.empty())
      return value_type {};

    std::ranges::sort(sorted);
    const auto middle = sorted.size() / 2;
    return sorted.size() % 2 != 0 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / static_cast<rep>(2);
  }
  [[nodiscard]] constexpr auto variance                () const
  {
    const auto count = values.size();
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
  auto write_console(std::ostream& stream) const -> std::ostream&
  {
    const auto real_time = mean().count();
    const auto cpu_time  = cpu_mean().count();
    return stream << "Benchmark Time(" << detail::unit<duration_type>() << ") CPU(" << detail::unit<duration_type>() << ") Iterations\n"
                  << name << ' ' << real_time << ' ' << cpu_time << ' ' << values.size() << '\n';
  }
  auto write_csv    (std::ostream& stream) const -> std::ostream&
  {
    const auto real_time = mean().count();
    const auto cpu_time  = cpu_mean().count();
    return stream << "name,iterations,real_time,cpu_time,time_unit\n"
                  << name << ',' << values.size() << ',' << real_time << ',' << cpu_time << ',' << detail::unit<duration_type>() << '\n';
  }
  auto write_json   (std::ostream& stream) const -> std::ostream&
  {
    const auto real_time = mean().count();
    const auto cpu_time  = cpu_mean().count();
    return stream << "{\"benchmarks\":[" 
      << "{\"name\":\""       << name 
      << "\",\"iterations\":" << values.size()
      << ",\"real_time\":"    << real_time
      << ",\"cpu_time\":"     << cpu_time
      << ",\"time_unit\":\""  << detail::unit<duration_type>()
      << "\"}]}\n";
  }

  std::string                name  ;
  std::vector<duration_type> values;
  std::vector<duration_type> cpu_values;
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
    constexpr auto unit = detail::unit<duration_type>();
    stream << "Benchmark Time(" << unit << ") CPU(" << unit << ") Iterations\n";
    for (const auto& entry : records_)
    {
      stream << entry.name << ' ' << entry.mean().count() << ' ' << entry.cpu_mean().count() << ' ' << entry.values.size() << '\n';
    }
    return stream;
  }
  auto                                      write_csv    (std::ostream& stream) const -> std::ostream&
  {
    constexpr auto unit = detail::unit<duration_type>();
    stream << "name,iterations,real_time,cpu_time,time_unit\n";
    for (const auto& entry : records_)
    {
      stream << entry.name << ',' << entry.values.size() << ',' << entry.mean().count() << ',' << entry.cpu_mean().count() << ',' << unit << '\n';
    }
    return stream;
  }
  auto                                      write_json   (std::ostream& stream) const -> std::ostream&
  {
    constexpr auto unit = detail::unit<duration_type>();
    stream << "{\"benchmarks\":[";
    auto first = true;
    for (const auto& entry : records_)
    {
      if (!first)
        stream << ',';
      first = false;
      stream << "{\"name\":\"" << entry.name
             << "\",\"iterations\":" << entry.values.size()
             << ",\"real_time\":" << entry.mean().count()
             << ",\"cpu_time\":" << entry.cpu_mean().count()
              << ",\"time_unit\":\"" << unit << "\"}";
    }
    return stream << "]}\n";
  }

private:
  template <detail::duration, typename>
  friend class session_recorder;

  [[nodiscard]] auto entry(const std::string_view name, const std::size_t iterations) -> record_type&
  {
    const auto entry = std::ranges::find_if(records_, [name] (const auto& record)
    {
      return record.name == name;
    });
    if (entry != records_.end())
      return *entry;

    records_.emplace_back(std::string {name}, std::vector<duration_type>(iterations), std::vector<duration_type>(iterations));
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
  {}

  template <typename function_type, typename... argument_types>
  requires std::invocable<function_type&, argument_types...>
  constexpr void record(const std::string_view name, function_type&& function, argument_types&&... arguments)
  {
    const auto start     = clock_type::now();
    const auto cpu_start = std::clock();
    std::invoke(function, std::forward<argument_types>(arguments)...);
    const auto cpu_end = std::clock();
    const auto end     = clock_type::now();
    auto&      result = session_.entry(name, iterations_);
    result.values    [index_] = std::chrono::duration_cast<duration_type>(end - start);
    result.cpu_values[index_] = std::chrono::duration_cast<duration_type>(std::chrono::duration<double> {static_cast<double>(cpu_end - cpu_start) / CLOCKS_PER_SEC});
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
[[nodiscard]] auto run(function_type&& function, const std::size_t iterations = 1) -> record<duration_type>
{
  auto callable = std::forward<function_type>(function);
  auto record = benchmark::record<duration_type> {"benchmark", std::vector<duration_type>(iterations), std::vector<duration_type>(iterations)};
  for (auto i = std::size_t {}; i < iterations; ++i)
  {
    const auto start     = clock_type::now();
    const auto cpu_start = std::clock();
    std::invoke(callable);
    const auto cpu_end = std::clock();
    const auto end     = clock_type::now();
    record.values    [i] = std::chrono::duration_cast<duration_type>(end - start);
    record.cpu_values[i] = std::chrono::duration_cast<duration_type>(std::chrono::duration<double> {static_cast<double>(cpu_end - cpu_start) / CLOCKS_PER_SEC});
  }
  return record;
}
template <detail::duration duration_type = std::chrono::duration<double, std::milli>,
          typename clock_type = std::chrono::steady_clock,
          typename function_type>
requires std::invocable<function_type&, session_recorder<duration_type, clock_type>&>
[[nodiscard]] auto run(function_type&& function, const std::size_t iterations = 1) -> session<duration_type>
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
