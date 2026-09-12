#include "ComplexityAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

ComplexityResult ComplexityAnalyzer::analyze(
    const std::vector<size_t>& input_sizes,
    const std::vector<double>& timings
) const {
    std::vector<double> x_values;
    std::vector<double> y_values;

    const size_t count = std::min(input_sizes.size(), timings.size());
    for (size_t index = 0; index < count; ++index) {
        if (input_sizes[index] > 1 && timings[index] > 0.0) {
            x_values.push_back(std::log(static_cast<double>(input_sizes[index])));
            y_values.push_back(std::log(timings[index]));
        }
    }

    ComplexityResult result;
    result.sample_count = x_values.size();
    if (x_values.size() < 3) {
        return result;
    }

    double mean_x = 0.0;
    double mean_y = 0.0;
    for (size_t index = 0; index < x_values.size(); ++index) {
        mean_x += x_values[index];
        mean_y += y_values[index];
    }
    mean_x /= static_cast<double>(x_values.size());
    mean_y /= static_cast<double>(y_values.size());

    double covariance = 0.0;
    double variance_x = 0.0;
    for (size_t index = 0; index < x_values.size(); ++index) {
        const double dx = x_values[index] - mean_x;
        covariance += dx * (y_values[index] - mean_y);
        variance_x += dx * dx;
    }
    if (variance_x <= 0.0) {
        return result;
    }

    result.exponent = covariance / variance_x;
    const double intercept = mean_y - result.exponent * mean_x;

    double total_sum_squares = 0.0;
    double residual_sum_squares = 0.0;
    for (size_t index = 0; index < x_values.size(); ++index) {
        const double predicted = intercept + result.exponent * x_values[index];
        const double residual = y_values[index] - predicted;
        total_sum_squares += (y_values[index] - mean_y) * (y_values[index] - mean_y);
        residual_sum_squares += residual * residual;
    }
    result.fit_quality = total_sum_squares > 0.0
        ? std::max(0.0, 1.0 - residual_sum_squares / total_sum_squares)
        : 1.0;

    std::ostringstream model;
    model << "n^" << std::fixed << std::setprecision(2) << result.exponent;
    result.observed_model = model.str();
    return result;
}
