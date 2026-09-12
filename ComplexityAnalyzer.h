#ifndef COMPLEXITY_ANALYZER_H
#define COMPLEXITY_ANALYZER_H

#include <cstddef>
#include <string>
#include <vector>

struct ComplexityResult {
    double exponent = 0.0;
    std::string observed_model = "Insufficient data";
    std::string theoretical_complexity;
    double fit_quality = 0.0;
    size_t sample_count = 0;
};

class ComplexityAnalyzer {
public:
    ComplexityResult analyze(
        const std::vector<size_t>& input_sizes,
        const std::vector<double>& timings
    ) const;
};

#endif
