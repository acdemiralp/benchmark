# Copilot Instructions

## Project Guidelines
- For this C++ benchmark project: use namespace `benchmark` instead of abbreviations, do not implement missing standard-library functionality locally, store timings as `std::chrono::duration` values, avoid Java-like `to_string()` and uncommon `print()` member APIs, keep CSV/JSON/console output close to Google Benchmark's formats, and prioritize a lightweight single-header design without main interference.
- Prefer concise APIs and implementation over verbose Google Benchmark-style formatting/parsing; keep reporters lightweight and avoid overcomplicated string handling.