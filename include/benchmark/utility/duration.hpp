#pragma once

#include <chrono>
#include <concepts>
#include <ratio>
#include <string_view>
#include <type_traits>

namespace benchmark
{
template <typename type>
struct         is_duration                                                : std::false_type {};
template <typename representation, typename period>
struct         is_duration<std::chrono::duration<representation, period>> : std::true_type  {};
template <typename type>
constexpr auto is_duration_v                                              = is_duration  <std::remove_cvref_t<type>>::value;
template <typename type>
concept        duration                                                   = is_duration_v<type>;

template <duration duration_type>
[[nodiscard]] consteval auto unit_symbol() noexcept -> std::string_view
{
  using period = typename duration_type::period;

  if      constexpr (std::same_as<period, std::atto                   >) return "as"   ;
  else if constexpr (std::same_as<period, std::femto                  >) return "fs"   ;
  else if constexpr (std::same_as<period, std::pico                   >) return "ps"   ;
  else if constexpr (std::same_as<period, std::nano                   >) return "ns"   ;
  else if constexpr (std::same_as<period, std::micro                  >) return "us"   ;
  else if constexpr (std::same_as<period, std::milli                  >) return "ms"   ;
  else if constexpr (std::same_as<period, std::centi                  >) return "cs"   ;
  else if constexpr (std::same_as<period, std::deci                   >) return "ds"   ;
  else if constexpr (std::same_as<period, std::ratio<1>               >) return "s"    ;
  else if constexpr (std::same_as<period, std::deca                   >) return "das"  ;
  else if constexpr (std::same_as<period, std::hecto                  >) return "hs"   ;
  else if constexpr (std::same_as<period, std::kilo                   >) return "ks"   ;
  else if constexpr (std::same_as<period, std::mega                   >) return "Ms"   ;
  else if constexpr (std::same_as<period, std::giga                   >) return "Gs"   ;
  else if constexpr (std::same_as<period, std::tera                   >) return "Ts"   ;
  else if constexpr (std::same_as<period, std::peta                   >) return "Ps"   ;
  else if constexpr (std::same_as<period, std::exa                    >) return "Es"   ;

  else if constexpr (std::same_as<period, std::chrono::minutes::period>) return "min"  ;
  else if constexpr (std::same_as<period, std::chrono::hours  ::period>) return "h"    ;
  else if constexpr (std::same_as<period, std::chrono::days   ::period>) return "d"    ;
  
  else                                                                   return "ticks";
}
}
