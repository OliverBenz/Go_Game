#pragma once

#include "Logger/Logger.hpp"

namespace go::gui {

//! Returns the logger instance based on the set up configuration.
Logging::Logger Logger();

} // namespace go::gui