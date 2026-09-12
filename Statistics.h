#ifndef STATISTICS_H
#define STATISTICS_H

#include <vector>
#include <stdint.h>

double get_average(
    const std::vector<uint64_t>&results
);


uint64_t get_minimum(
    const std::vector<uint64_t>&results
);


uint64_t get_maximum(
    const std::vector<uint64_t>&results
);


double get_median(
    std::vector<uint64_t>results
);

double get_percentile(
    std::vector<uint64_t> values,
    double percentile
);


double get_standard_deviation(
    const std::vector<uint64_t>&results,
    double average
);

double get_coefficient_of_variation(
    const std::vector<uint64_t>& results,
    double average
);

#endif
