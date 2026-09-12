#ifndef SORTING_ALGORITHMS_H
#define SORTING_ALGORITHMS_H

#include <algorithm>
#include <vector>

inline void bubble_sort(std::vector<int>& values) {
    for (size_t outer = 0; outer + 1 < values.size(); ++outer) {
        bool swapped = false;
        for (size_t inner = 0; inner + 1 < values.size() - outer; ++inner) {
            if (values[inner] > values[inner + 1]) {
                std::swap(values[inner], values[inner + 1]);
                swapped = true;
            }
        }
        if (!swapped) {
            return;
        }
    }
}

inline void insertion_sort(std::vector<int>& values) {
    for (size_t index = 1; index < values.size(); ++index) {
        const int value = values[index];
        size_t position = index;
        while (position > 0 && values[position - 1] > value) {
            values[position] = values[position - 1];
            --position;
        }
        values[position] = value;
    }
}

inline void selection_sort(std::vector<int>& values) {
    for (size_t index = 0; index < values.size(); ++index) {
        size_t minimum = index;
        for (size_t candidate = index + 1; candidate < values.size(); ++candidate) {
            if (values[candidate] < values[minimum]) {
                minimum = candidate;
            }
        }
        std::swap(values[index], values[minimum]);
    }
}

inline void merge_sort_impl(
    std::vector<int>& values,
    std::vector<int>& temporary,
    size_t left,
    size_t right
) {
    if (right - left <= 1) {
        return;
    }
    const size_t middle = left + (right - left) / 2;
    merge_sort_impl(values, temporary, left, middle);
    merge_sort_impl(values, temporary, middle, right);
    std::merge(
        values.begin() + static_cast<std::ptrdiff_t>(left),
        values.begin() + static_cast<std::ptrdiff_t>(middle),
        values.begin() + static_cast<std::ptrdiff_t>(middle),
        values.begin() + static_cast<std::ptrdiff_t>(right),
        temporary.begin() + static_cast<std::ptrdiff_t>(left)
    );
    std::copy(
        temporary.begin() + static_cast<std::ptrdiff_t>(left),
        temporary.begin() + static_cast<std::ptrdiff_t>(right),
        values.begin() + static_cast<std::ptrdiff_t>(left)
    );
}

inline void merge_sort(std::vector<int>& values, std::vector<int>& temporary) {
    temporary.resize(values.size());
    merge_sort_impl(values, temporary, 0, values.size());
}

inline size_t quick_partition(std::vector<int>& values, size_t left, size_t right) {
    const int pivot = values[left + (right - left) / 2];
    size_t first = left;
    size_t last = right;
    while (true) {
        while (values[first] < pivot) {
            ++first;
        }
        while (values[last] > pivot) {
            --last;
        }
        if (first >= last) {
            return last;
        }
        std::swap(values[first], values[last]);
        ++first;
        --last;
    }
}

inline void quick_sort_impl(std::vector<int>& values, size_t left, size_t right) {
    if (left >= right) {
        return;
    }
    const size_t split = quick_partition(values, left, right);
    quick_sort_impl(values, left, split);
    quick_sort_impl(values, split + 1, right);
}

inline void quick_sort(std::vector<int>& values) {
    if (!values.empty()) {
        quick_sort_impl(values, 0, values.size() - 1);
    }
}

inline void heap_sort(std::vector<int>& values) {
    std::make_heap(values.begin(), values.end());
    std::sort_heap(values.begin(), values.end());
}

#endif
