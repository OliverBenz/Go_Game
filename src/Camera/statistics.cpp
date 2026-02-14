#include "statistics.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

// TODO: Check all this
namespace go::camera {

double mean(const std::vector<double>& v) {
    if (v.empty()) {
        return 0.0;
    };
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

double variance(const std::vector<double>& v) {
    if (v.size() < 2)
        return 0.0;

    double m = mean(v);

    double accum = 0.0;
    for (double x : v) {
        double d = x - m;
        accum += d * d;
    }

    return accum / static_cast<double>(v.size());  // population variance
}

double stddev(const std::vector<double>& v) {
    return std::sqrt(variance(v));
}

double median(std::vector<double> values)
{
    assert(!values.empty());

    const size_t n = values.size();
    const size_t mid = n / 2;

    std::nth_element(values.begin(), values.begin() + mid, values.end());
    double m = values[mid];

    if (n % 2 == 0) {
        std::nth_element(values.begin(), values.begin() + mid - 1, values.end());
        m = 0.5 * (m + values[mid - 1]);
    }

    return m;
}


}
