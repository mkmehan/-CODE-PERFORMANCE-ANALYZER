#ifndef SORTING_DATA_H
#define SORTING_DATA_H

#include "../Benchmark.h"

#include <algorithm>
#include <random>
#include <vector>

inline std::vector<int> get_sorting_input(
    size_t size,
    InputDataCase input_case,
    uint32_t seed
) {
    std::vector<int> values(size);

    if (input_case == InputDataCase::Sorted ||
        input_case == InputDataCase::NearlySorted) {
        for (size_t index = 0; index < size; ++index) {
            values[index] = static_cast<int>(index);
        }

        if (input_case == InputDataCase::NearlySorted && size > 1) {
            std::mt19937 generator(seed);
            const size_t swaps = std::max<size_t>(1, size / 20);
            std::uniform_int_distribution<size_t> position(0, size - 2);
            for (size_t index = 0; index < swaps; ++index) {
                const size_t left = position(generator);
                std::swap(values[left], values[left + 1]);
            }
        }
        return values;
    }

    if (input_case == InputDataCase::ReverseSorted) {
        for (size_t index = 0; index < size; ++index) {
            values[index] = static_cast<int>(size - index);
        }
        return values;
    }

    if (input_case == InputDataCase::AllEqual) {
        std::fill(values.begin(), values.end(), 42);
        return values;
    }

    std::mt19937 generator(seed);
    const int maximum = input_case == InputDataCase::ManyDuplicates ? 10 : 1000000;
    std::uniform_int_distribution<int> distribution(0, maximum);
    for (int& value : values) {
        value = distribution(generator);
    }
    return values;
}

#endif
