#pragma once

#include <numeric>

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
