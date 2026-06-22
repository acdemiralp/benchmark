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
}  // namespace detail

template <detail::duration duration_type = std::chrono::duration<double, std::nano>>
struct record
{
  using value_type = duration_type;
  using rep        = typename duration_type::rep;

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
  struct index_type
  {
    std::size_t value;
  };
  struct iterations_type
  {
    std::size_t value;
  };

  session_recorder  (const index_type index,
                     const iterations_type iterations,
                     session<duration_type>& session) noexcept
  : index_(index.value), iterations_(iterations.value), session_(session)
  {

  }
  session_recorder           (const session_recorder& ) = delete;
  session_recorder           (      session_recorder&&) = delete;
  ~session_recorder          () noexcept                = default;
  auto operator=(const session_recorder& ) -> session_recorder& = delete;
  auto operator=(      session_recorder&&) -> session_recorder& = delete;

  template <typename function_type>
  requires std::invocable<function_type&>
  constexpr void record(const std::string_view name, function_type&& function)
  {
    const auto start = clock_type::now();
    std::invoke(std::forward<function_type>(function));
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
    auto recorder = session_recorder<duration_type, clock_type> {
      typename session_recorder<duration_type, clock_type>::index_type      {i},
      typename session_recorder<duration_type, clock_type>::iterations_type {iterations},
      session};
    std::invoke(session_callable, recorder);
  }
  return session;
}

namespace detail
{
template <duration duration_type>
void write_console_record(std::ostream& stream, const record<duration_type>& record)
{
  stream << record.name << ' '
         << mean(record.values.begin(), record.values.end()).count() << ' '
         << record.values.size() << '\n';
}

template <duration duration_type>
void write_csv_record(std::ostream& stream, const record<duration_type>& record)
{
  stream << record.name << ','
         << record.values.size() << ','
         << mean(record.values.begin(), record.values.end()).count() << ','
         << unit<duration_type>() << '\n';
}

template <duration duration_type>
void write_json_record(std::ostream& stream, const record<duration_type>& record)
{
  stream << R"({"name":")" << record.name
         << R"(","iterations":)" << record.values.size()
         << ",\"real_time\":" << mean(record.values.begin(), record.values.end()).count()
         << R"(,"time_unit":")" << unit<duration_type>() << "\"}";
}
}  // namespace detail

template <detail::duration duration_type>
auto write_console(std::ostream& stream, const record<duration_type>& record) -> std::ostream&
{
  stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n";
  detail::write_console_record(stream, record);
  return stream;
}

template <detail::duration duration_type>
auto write_console(std::ostream& stream, const session<duration_type>& session) -> std::ostream&
{
  stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n";
  for (const auto& record : session.records())
  {
    detail::write_console_record(stream, record);
  }
  return stream;
}

template <detail::duration duration_type>
auto write_csv(std::ostream& stream, const record<duration_type>& record) -> std::ostream&
{
  stream << "name,iterations,real_time,time_unit\n";
  detail::write_csv_record(stream, record);
  return stream;
}

template <detail::duration duration_type>
auto write_csv(std::ostream& stream, const session<duration_type>& session) -> std::ostream&
{
  stream << "name,iterations,real_time,time_unit\n";
  for (const auto& record : session.records())
  {
    detail::write_csv_record(stream, record);
  }
  return stream;
}

template <detail::duration duration_type>
auto write_json(std::ostream& stream, const record<duration_type>& record) -> std::ostream&
{
  stream << "{\"benchmarks\":[";
  detail::write_json_record(stream, record);
  return stream << "]}\n";
}

template <detail::duration duration_type>
auto write_json(std::ostream& stream, const session<duration_type>& session) -> std::ostream&
{
  stream << "{\"benchmarks\":[";
  auto first = true;
  for (const auto& record : session.records())
  {
    if (!first)
    {
      stream << ',';
    }
    first = false;
    detail::write_json_record(stream, record);
  }
  return stream << "]}\n";
}
}  // namespace benchmark

#endif
