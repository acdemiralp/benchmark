#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <numeric>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace benchmark
{
namespace detail
{
template <typename type>
inline constexpr auto is_duration = false;

template <typename representation, typename period>
inline constexpr auto is_duration<std::chrono::duration<representation, period>> = true;

template <typename type>
concept duration = is_duration<type> && requires
{
  typename type::rep;
  requires std::floating_point<typename type::rep>;
};

template <duration duration_type>
[[nodiscard]] consteval std::string_view unit() noexcept
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

inline std::ostream& write_csv_name(std::ostream& stream, const std::string_view name)
{
  stream << '"';
  for (const auto character : name)
  {
    if (character == '"')
      stream << "\"\"";
    else
      stream << character;
  }
  return stream << '"';
}

inline std::ostream& write_json_name(std::ostream& stream, const std::string_view name)
{
  stream << '"';
  for (const auto character : name)
  {
    if (character == '"' || character == '\\')
      stream << '\\';
    stream << character;
  }
  return stream << '"';
}
}

template <detail::duration duration_type = std::chrono::duration<double, std::nano>>
struct record
{
  using value_type = duration_type;
  using rep        = typename duration_type::rep;

  std::string                name  ;
  std::vector<duration_type> values;
};

template <detail::duration duration_type>
[[nodiscard]] constexpr duration_type sum(const record<duration_type>& record)
{
  return std::reduce(record.values.begin(), record.values.end(), duration_type {});
}

template <detail::duration duration_type>
[[nodiscard]] constexpr duration_type mean(const record<duration_type>& record)
{
  using rep = typename duration_type::rep;
  return record.values.empty() ? duration_type {} : sum(record) / static_cast<rep>(record.values.size());
}

template <detail::duration duration_type>
[[nodiscard]] constexpr duration_type minimum(const record<duration_type>& record)
{
  return record.values.empty() ? duration_type {} : *std::min_element(record.values.begin(), record.values.end());
}

template <detail::duration duration_type>
[[nodiscard]] constexpr duration_type maximum(const record<duration_type>& record)
{
  return record.values.empty() ? duration_type {} : *std::max_element(record.values.begin(), record.values.end());
}

template <detail::duration duration_type>
[[nodiscard]] constexpr duration_type median(const record<duration_type>& record)
{
  if (record.values.empty())
    return duration_type {};

  auto sorted = record.values;
  std::sort(sorted.begin(), sorted.end());
  const auto middle = sorted.size() / 2;
  if (sorted.size() % 2 != 0)
    return sorted[middle];

  using rep = typename duration_type::rep;
  return (sorted[middle - 1] + sorted[middle]) / static_cast<rep>(2);
}

template <detail::duration duration_type>
[[nodiscard]] constexpr typename duration_type::rep variance(const record<duration_type>& record)
{
  using rep = typename duration_type::rep;
  if (record.values.empty())
    return rep {};

  const auto average = mean(record).count();
  const auto total = std::transform_reduce(record.values.begin(), record.values.end(), rep {}, std::plus {},
                                           [average] (const duration_type value) constexpr noexcept
                                           {
                                             const auto difference = value.count() - average;
                                             return difference * difference;
                                           });
  return total / static_cast<rep>(record.values.size());
}

template <detail::duration duration_type>
[[nodiscard]] duration_type standard_deviation(const record<duration_type>& record) noexcept
{
  return duration_type {std::sqrt(variance(record))};
}

template <detail::duration duration_type>
[[nodiscard]] typename duration_type::rep coefficient_of_variation(const record<duration_type>& record) noexcept
{
  using rep = typename duration_type::rep;
  const auto average = mean(record).count();
  return average == rep {} ? rep {} : standard_deviation(record).count() / average;
}

template <detail::duration duration_type = std::chrono::duration<double, std::nano>>
class  session
{
public:
  using record_type = record<duration_type>;

  [[nodiscard]] constexpr std::size_t iterations() const noexcept
  {
    return records_.empty() ? std::size_t {} : records_.front().values.size();
  }
  [[nodiscard]] constexpr const std::vector<record_type>& records() const noexcept
  {
    return records_;
  }

private:
  template <detail::duration, typename>
  friend class session_recorder;

  [[nodiscard]] record_type& record(const std::string_view name, const std::size_t iterations)
  {
    const auto key             = std::string {name};
    const auto [index, insert] = indices_.try_emplace(key, records_.size());
    if (insert)
      records_.emplace_back(index->first, std::vector<duration_type>(iterations));
    return records_[index->second];
  }

  std::vector<record_type>                     records_;
  std::unordered_map<std::string, std::size_t> indices_;
};

template <detail::duration duration_type = std::chrono::duration<double, std::nano>, typename clock_type = std::chrono::steady_clock>
class  session_recorder
{
public:
  explicit constexpr session_recorder  (const std::size_t index, const std::size_t iterations, session<duration_type>& session) noexcept
  : index_(index), iterations_(iterations), session_(session)
  {

  }
  constexpr session_recorder           (const session_recorder&  that) = delete;
  constexpr session_recorder           (      session_recorder&& temp) = delete;
  constexpr ~session_recorder          () noexcept                     = default;
  constexpr session_recorder& operator=(const session_recorder&  that) = delete;
  constexpr session_recorder& operator=(      session_recorder&& temp) = delete;

  template <typename function_type>
  requires std::invocable<function_type&>
  constexpr void record(const std::string_view name, function_type&& function)
  {
    const auto start = clock_type::now();
    std::invoke(function);
    const auto end    = clock_type::now();
    auto&      result = session_.record(name, iterations_);
    result.values[index_] = std::chrono::duration_cast<duration_type>(end - start);
  }

protected:
  const std::size_t       index_     ;
  const std::size_t       iterations_;
  session<duration_type>& session_   ;
};

template<detail::duration duration_type = std::chrono::duration<double, std::nano>, typename clock_type = std::chrono::steady_clock, typename function_type>
requires std::invocable<function_type&>
[[nodiscard]] record<duration_type>   run(function_type&& function, const std::size_t iterations = 1)
{
  auto record = benchmark::record<duration_type> {"benchmark", std::vector<duration_type>(iterations)};
  for (auto i = std::size_t {}; i < iterations; ++i)
  {
    const auto start = clock_type::now();
    std::invoke(function);
    const auto end   = clock_type::now();
    record.values[i] = std::chrono::duration_cast<duration_type>(end - start);
  }
  return record;
}
template<detail::duration duration_type = std::chrono::duration<double, std::nano>, typename clock_type = std::chrono::steady_clock, typename function_type>
requires std::invocable<function_type&, session_recorder<duration_type, clock_type>&>
[[nodiscard]] session<duration_type>  run(function_type&& function, const std::size_t iterations = 1)
{
  auto session = benchmark::session<duration_type> {};
  for (auto i = std::size_t {}; i < iterations; ++i)
  {
    auto recorder = session_recorder<duration_type, clock_type> {i, iterations, session};
    std::invoke(function, recorder);
  }
  return session;
}

template <detail::duration duration_type>
std::ostream& write_console(std::ostream& stream, const record<duration_type>& record)
{
  return stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n"
                << record.name << ' ' << mean(record).count() << ' ' << record.values.size() << '\n';
}

template <detail::duration duration_type>
std::ostream& write_console(std::ostream& stream, const session<duration_type>& session)
{
  stream << "Benchmark Time(" << detail::unit<duration_type>() << ") Iterations\n";
  for (const auto& record : session.records())
    stream << record.name << ' ' << mean(record).count() << ' ' << record.values.size() << '\n';
  return stream;
}

template <detail::duration duration_type>
std::ostream& write_csv(std::ostream& stream, const record<duration_type>& record)
{
  stream << "name,iterations,real_time,time_unit\n";
  detail::write_csv_name(stream, record.name);
  return stream << ',' << record.values.size() << ',' << mean(record).count() << ',' << detail::unit<duration_type>() << '\n';
}

template <detail::duration duration_type>
std::ostream& write_csv(std::ostream& stream, const session<duration_type>& session)
{
  stream << "name,iterations,real_time,time_unit\n";
  for (const auto& record : session.records())
  {
    detail::write_csv_name(stream, record.name);
    stream << ',' << record.values.size() << ',' << mean(record).count() << ',' << detail::unit<duration_type>() << '\n';
  }
  return stream;
}

template <detail::duration duration_type>
std::ostream& write_json(std::ostream& stream, const record<duration_type>& record)
{
  stream << "{\"benchmarks\":[{\"name\":";
  detail::write_json_name(stream, record.name);
  return stream << ",\"iterations\":" << record.values.size()
                << ",\"real_time\":" << mean(record).count() << ",\"time_unit\":\"" << detail::unit<duration_type>() << "\"}]}\n";
}

template <detail::duration duration_type>
std::ostream& write_json(std::ostream& stream, const session<duration_type>& session)
{
  stream << "{\"benchmarks\":[";
  for (auto i = std::size_t {}; i < session.records().size(); ++i)
  {
    const auto& record = session.records()[i];
    if (i != 0)
      stream << ',';
    stream << "{\"name\":";
    detail::write_json_name(stream, record.name);
    stream << ",\"iterations\":" << record.values.size()
           << ",\"real_time\":" << mean(record).count() << ",\"time_unit\":\"" << detail::unit<duration_type>() << "\"}";
  }
  return stream << "]}\n";
}
}
