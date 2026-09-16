#include <cstddef>

// ============================================================================
// Code Performance Analyzer - Custom Sorting (Extension Preview)
// Contract:
//   data -> Pointer to array of integer elements to sort in-place
//   size -> Number of elements in data
// ============================================================================

void sort_algorithm(int* data, int size) {
    for (int i = 0; i < size - 1; ++i) {
        for (int j = 0; j < size - i - 1; ++j) {
            if (data[j] > data[j + 1]) {
                int tmp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = tmp;
            }
        }
    }
}

