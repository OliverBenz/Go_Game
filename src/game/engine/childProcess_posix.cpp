
//! On POSIX: SIGTERM, then SIGKILL after grace period.
void terminate(std::chrono::milliseconds gracePeriod = std::chrono::milliseconds{500}) noexcept;
