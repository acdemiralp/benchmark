#ifndef BENCHMARK_BENCHMARK_HPP_
#define BENCHMARK_BENCHMARK_HPP_

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef BM_MPI_SUPPORT
#include <mpi.h>
#endif


namespace bm
{
namespace detail
{
template <typename type, typename period, typename callable>
type measure(callable&& function)
{
  const auto start = std::chrono::steady_clock::now();
  std::invoke(std::forward<callable>(function));
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<type, period>(end - start).count();
}
}

template <typename type = double>
struct record
{
  [[nodiscard]] type mean() const noexcept
  {
    if (values.empty())
      return type{};

    return std::accumulate(values.begin(), values.end(), type{}) / static_cast<type>(values.size());
  }
  [[nodiscard]] type variance() const noexcept
  {
    if (values.empty())
      return type{};

    const auto average = mean();
    return std::accumulate(values.begin(), values.end(), type{},
      [average] (const type accumulator, const type value)
      {
        const auto difference = value - average;
        return accumulator + difference * difference;
      }) / static_cast<type>(values.size());
  }
  [[nodiscard]] type standard_deviation() const noexcept
  {
    return std::sqrt(variance());
  }

  [[nodiscard]] std::string to_string() const
  {
    std::ostringstream stream;
    stream.precision(std::numeric_limits<type>::max_digits10);
    stream << name << ",";
    for (const auto value : values)
      stream << value << ",";
    stream << mean() << "," << variance() << "," << standard_deviation();
    return stream.str();
  }
  void to_csv(const std::string& filepath) const
  {
    std::ofstream stream(filepath);
    if (!stream)
      throw std::runtime_error("failed to open benchmark output file");

    stream << "name,";
    for (std::size_t i = 0; i < values.size(); ++i)
      stream << "run_" << i << ",";
    stream << "mean,variance,standard deviation\n";
    stream << to_string();
  }

  std::string       name  ;
  std::vector<type> values;
};

template <typename type = double>
struct session
{
  virtual ~session() = default;

  [[nodiscard]] virtual std::string to_string() const
  {
    std::ostringstream stream;
    for (auto record = records.begin(); record != records.end(); ++record)
    {
      stream << record->to_string();
      if (std::next(record) != records.end())
        stream << "\n";
    }
    return stream.str();
  }
  virtual void to_csv(const std::string& filepath) const
  {
    std::ofstream stream(filepath);
    if (!stream)
      throw std::runtime_error("failed to open benchmark output file");

    stream << "name,";
    const auto run_count = records.empty() ? std::size_t{} : records.front().values.size();
    for (std::size_t i = 0; i < run_count; ++i)
      stream << "run_" << i << ",";
    stream << "mean,variance,standard deviation\n";
    if (!records.empty())
      stream << to_string();
  }

  std::vector<record<type>> records;
};

#ifdef BM_MPI_SUPPORT
template <typename type = double>
class  mpi_session : public session<type>
{
public:
  mpi_session           (MPI_Comm communicator = MPI_COMM_WORLD, std::int32_t master_rank = 0) : communicator_(communicator), master_rank_(master_rank)
  {
    MPI_Comm_rank(communicator_, &rank_);
    MPI_Comm_size(communicator_, &size_);
  }
  mpi_session           (const mpi_session&  that) = default;
  mpi_session           (      mpi_session&& temp) = default;
 ~mpi_session           ()                         = default;
  mpi_session& operator=(const mpi_session&  that) = default;
  mpi_session& operator=(      mpi_session&& temp) = default;
  
  void gather()
  {
    std::ostringstream stream;
    for (const auto& record : this->records)
      stream << rank_ << "," << record.to_string() << "\n";
    std::string local_string = stream.str();
    std::int32_t local_size = static_cast<std::int32_t>(local_string.size());
    
    std::vector<std::int32_t> sizes        (size_);
    std::vector<std::int32_t> displacements(size_);
    std::int32_t              counter = 0;
    MPI_Gather (&local_size, 1, MPI_INT, sizes.data(), 1, MPI_INT, master_rank_, communicator_);
    for (auto i = 0; i < size_; ++i)
      displacements[i] = counter, counter += sizes[i];
    gathered_.resize(counter);
    MPI_Gatherv(local_string.data(), local_size, MPI_CHAR, gathered_.data(), sizes.data(), displacements.data(), MPI_CHAR, master_rank_, communicator_);
  }
  [[nodiscard]] virtual std::string to_string() const override
  {
    return rank_ == master_rank_ ? gathered_ : session<type>::to_string();
  }
  virtual void to_csv(const std::string& filepath) const override
  {
    if (rank_ != master_rank_)
      return;

    std::ofstream stream(filepath);
    if (!stream)
      throw std::runtime_error("failed to open benchmark output file");

    stream << "rank,name,";
    const auto run_count = this->records.empty() ? std::size_t{} : this->records.front().values.size();
    for (std::size_t i = 0; i < run_count; ++i)
      stream << "run_" << i << ",";
    stream << "mean,variance,standard deviation\n";
    if (!this->records.empty())
      stream << to_string();
  }
  
protected:
  MPI_Comm     communicator_;
  std::int32_t master_rank_ ;
  std::int32_t rank_        ;
  std::int32_t size_        ;
  std::string  gathered_    ;
};
#endif

template <typename type = double, typename period = std::milli>
class  session_recorder
{
public:
  explicit session_recorder  (const std::size_t index, const std::size_t iterations, session<type>& session) 
  : index_(index), iterations_(iterations), session_(session)
  {

  }
  session_recorder           (const session_recorder&  that) = delete ;
  session_recorder           (      session_recorder&& temp) = default;
  virtual ~session_recorder  ()                              = default;
  session_recorder& operator=(const session_recorder&  that) = delete ;
  session_recorder& operator=(      session_recorder&& temp) = default;
  
  template <typename callable>
  void record(const std::string_view name, callable&& function)
  requires std::invocable<callable&>
  {
    auto entry = std::find_if(session_.records.begin(), session_.records.end(),
      [name] (const bm::record<type>& record) { return record.name == name; });
    if (entry == session_.records.end())
    {
      session_.records.push_back({std::string(name), std::vector<type>(iterations_)});
      entry = std::prev(session_.records.end());
    }

    entry->values[index_] = detail::measure<type, period>(std::forward<callable>(function));
  }

protected:
  const std::size_t index_     ;
  const std::size_t iterations_;
  session<type>&    session_   ;
};

template<typename type = double, typename period = std::milli, typename callable>
record<type> run(callable&& function, const std::size_t iterations = 1)
requires std::invocable<callable&>
{
  record<type> record {"benchmark", std::vector<type>(iterations)};
  for (std::size_t i = 0; i < iterations; ++i)
    record.values[i] = detail::measure<type, period>(function);

  return record;
}
template<typename type = double, typename period = std::milli, typename callable>
session<type> run(callable&& function, const std::size_t iterations = 1)
requires std::invocable<callable&, session_recorder<type, period>&>
{
  session<type> session;
  for(std::size_t i = 0; i < iterations; ++i)
  {
    session_recorder<type, period> recorder(i, iterations, session);
    std::invoke(function, recorder);
  }
  return session;
}
#ifdef BM_MPI_SUPPORT
template<typename type = double, typename period = std::milli, typename callable>
mpi_session<type> run_mpi(callable&& function, const std::size_t iterations = 1, const MPI_Comm communicator = MPI_COMM_WORLD, const std::int32_t master_rank = 0)
requires std::invocable<callable&, session_recorder<type, period>&>
{
  mpi_session<type> session(communicator, master_rank);
  for (std::size_t i = 0; i < iterations; ++i)
  {
    session_recorder<type, period> recorder(i, iterations, session);
    std::invoke(function, recorder);
  }
  return session;
}
#endif
}

#endif
