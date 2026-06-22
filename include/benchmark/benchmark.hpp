#ifndef BENCHMARK_BENCHMARK_HPP
#define BENCHMARK_BENCHMARK_HPP

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
#include <unordered_map>
#include <utility>
#include <vector>

namespace benchmark
{
namespace detail
{
template <typename type>
inline constexpr auto is_duration                                                = false;
template <typename representation, typename period>
inline constexpr auto is_duration<std::chrono::duration<representation, period>> = true;

template <typename type>
concept duration = is_duration<type> && requires
{
  typename type::rep;
  requires std::floating_point<typename type::rep>;
};

template <duration duration_type>
[[nodiscard]] consteval auto unit() noexcept -> std::string_view
{
  using period = typename duration_type::period;
  if constexpr (std::same_as<period, std::nano>)
  {
    return "ns";
  }
  else if constexpr (std::same_as<period, std::micro>)
  {
    return "us";
  }
  else if constexpr (std::same_as<period, std::milli>)
  {
    return "ms";
  }
  else if constexpr (std::same_as<period, std::ratio<1>>)
  {
    return "s";
  }
  else
  {
    return "ticks";
  }
}

enum class report_format
{
  console,
  csv,
  json
};
}  // namespace detail

template <detail::duration duration_type>
struct record;

namespace detail
{
template <duration duration_type>
auto write_record(std::ostream& stream, const benchmark::record<duration_type>& record, report_format format) -> std::ostream&;
}  // namespace detail

template <detail::duration duration_type = std::chrono::duration<double, std::nano>>
struct record
{
  using value_type = duration_type;
  using rep        = typename duration_type::rep;

  auto write_console(std::ostream& stream) const -> std::ostream&
  {
    stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n";
    return detail::write_record(stream, *this, detail::report_format::console);
  }
  auto write_csv    (std::ostream& stream) const -> std::ostream&
  {
    stream << "name,iterations,real_time,time_unit\n";
    return detail::write_record(stream, *this, detail::report_format::csv);
  }
  auto write_json   (std::ostream& stream) const -> std::ostream&
  {
    stream << "{\"benchmarks\":[";
    detail::write_record(stream, *this, detail::report_format::json);
    return stream << "]}\n";
  }

  std::string                name  ;
  std::vector<duration_type> values;
};

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] constexpr auto mean(iterator first, iterator last)
{
  using value_type = std::iter_value_t<iterator>;
  using rep        = typename value_type::rep;
  const auto result = std::accumulate(first, last, value_type {});
  return result == value_type {} ? result : result / static_cast<rep>(std::distance(first, last));
}

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] constexpr auto minimum(iterator first, iterator last)
{
  return first == last ? std::iter_value_t<iterator> {} : *std::min_element(first, last);
}

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] constexpr auto maximum(iterator first, iterator last)
{
  return first == last ? std::iter_value_t<iterator> {} : *std::max_element(first, last);
}

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] constexpr auto median(iterator first, iterator last)
{
  using value_type = std::iter_value_t<iterator>;
  using rep        = typename value_type::rep;

  auto sorted = std::vector<value_type>(first, last);
  if (sorted.empty())
  {
    return value_type {};
  }

  std::sort(sorted.begin(), sorted.end());
  const auto middle = sorted.size() / 2;
  return sorted.size() % 2 != 0 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / static_cast<rep>(2);
}

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] constexpr auto variance(iterator first, iterator last)
{
  using value_type = std::iter_value_t<iterator>;
  using rep        = typename value_type::rep;
  const auto average = mean(first, last).count();
  const auto result  = std::transform_reduce(first, last, rep {}, std::plus {},
                                             [average] (const value_type value) constexpr noexcept
                                             {
                                               const auto difference = value.count() - average;
                                               return difference * difference;
                                             });
  return result == rep {} ? result : result / static_cast<rep>(std::distance(first, last));
}

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] auto standard_deviation(iterator first, iterator last) noexcept
{
  return std::iter_value_t<iterator> {std::sqrt(variance(first, last))};
}

template <std::forward_iterator iterator>
requires detail::duration<std::iter_value_t<iterator>>
[[nodiscard]] auto coefficient_of_variation(iterator first, iterator last) noexcept
{
  using rep = typename std::iter_value_t<iterator>::rep;
  const auto average = mean(first, last).count();
  return average == rep {} ? rep {} : standard_deviation(first, last).count() / average;
}

template <detail::duration duration_type = std::chrono::duration<double, std::nano>>
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
      detail::write_record(stream, record, detail::report_format::console);
    }
    return stream;
  }
  auto                                      write_csv    (std::ostream& stream) const -> std::ostream&
  {
    stream << "name,iterations,real_time,time_unit\n";
    for (const auto& record : records_)
    {
      detail::write_record(stream, record, detail::report_format::csv);
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
      {
        stream << ',';
      }
      first = false;
      detail::write_record(stream, record, detail::report_format::json);
    }
    return stream << "]}\n";
  }

private:
  template <detail::duration, typename>
  friend class session_recorder;

  [[nodiscard]] auto entry(const std::string_view name, const std::size_t iterations) -> record_type&
  {
    const auto key             = std::string {name};
    const auto [index, insert] = indices_.try_emplace(key, records_.size());
    if (insert)
    {
      records_.emplace_back(index->first, std::vector<duration_type>(iterations));
    }
    return records_[index->second];
  }

  std::vector<record_type>                     records_;
  std::unordered_map<std::string, std::size_t> indices_;
};

template <detail::duration duration_type = std::chrono::duration<double, std::nano>,
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

template <detail::duration duration_type = std::chrono::duration<double, std::nano>,
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
template <detail::duration duration_type = std::chrono::duration<double, std::nano>,
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

namespace detail
{
template <duration duration_type>
auto write_record(std::ostream& stream, const record<duration_type>& record, report_format format) -> std::ostream&
{
  const auto real_time = mean(record.values.begin(), record.values.end()).count();
  if (format == report_format::console)
  {
    return stream << record.name << ' ' << real_time << ' ' << record.values.size() << '\n';
  }
  if (format == report_format::csv)
  {
    return stream << record.name << ',' << record.values.size() << ',' << real_time << ',' << unit<duration_type>() << '\n';
  }

  return stream << R"({"name":")" << record.name
                << R"(","iterations":)" << record.values.size()
                << ",\"real_time\":" << real_time
                << R"(,"time_unit":")" << unit<duration_type>() << "\"}";
}
}  // namespace detail
}  // namespace benchmark

#endif
