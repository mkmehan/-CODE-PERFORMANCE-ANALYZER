#include <vector>
#include <cstddef>

// ============================================================================
// Code Performance Analyzer - Custom Search Algorithm (Dataset Only)
// Contract:
//   data   -> Pointer to array of integer elements
//   size   -> Number of elements in data (int or size_t)
// Return:
//   Computed integer metric, index, or status code
// ============================================================================

int search_algorithm(const int* data, int size) {
    int max_val = (size > 0) ? data[0] : -1;
    for (int i = 1; i < size; ++i) {
        if (data[i] > max_val) {
            max_val = data[i];
        }
    }
    return max_val;
}

