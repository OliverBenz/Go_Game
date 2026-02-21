#pragma once

#include <vector>

namespace go::camera {

double mean(const std::vector<double>& v);
double variance(const std::vector<double>& v);
double stddev(const std::vector<double>& v);

double median(std::vector<double> values);

} // namespace go::camera
