#include <vector>

// Code Performance Analyzer - Dataset Operation Contract (Template 2)
//
// Required Function Contract:
//   long long custom_algorithm(const int* data, int size)
//
// Benchmark Semantics:
//   Array Maximum Element Search: Scans array and returns the 0-based index
//   of the maximum element in data[0..size-1] (first occurrence if duplicates).
//
// Parameters:
//   data   -> Pointer to the array of integers
//   size   -> Total number of elements in the array
//
// Return:
//   0-based index of maximum element (0 <= index < size), or -1 if size <= 0
//
long long custom_algorithm(const int* data, int size) {
    if (size <= 0) return -1;
    int max_idx = 0;
    int max_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = i;
        }
    }
    return max_idx;
}
