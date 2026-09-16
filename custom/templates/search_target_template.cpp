#include <vector>
#include <cstddef>

// ============================================================================
// Code Performance Analyzer - Custom Search Algorithm (With Target)
// Contract:
//   data   -> Pointer to array of integer elements
//   size   -> Number of elements in data (int or size_t)
//   target -> Value to search for
// Return:
//   0-based index if target is found, or -1 if not found.
// ============================================================================

int search_algorithm(const int* data, int size, int target) {
    for (int i = 0; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return -1;
}

