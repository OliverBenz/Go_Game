
//! On Windows: graceful mechanisms are limited; backend may call TerminateProcess.
void terminate(std::chrono::milliseconds gracePeriod = std::chrono::milliseconds{500}) noexcept;
