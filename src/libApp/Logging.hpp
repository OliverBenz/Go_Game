#pragma once

#include "Logger/Logger.hpp"

namespace go::app {

//! Returns the logger instance based on the set up configuration.
Logging::Logger Logger();

} // namespace go::app