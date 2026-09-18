#include <vector>

// Code Performance Analyzer - Targeted Search Contract (Template 1)
//
// Required Function Contract:
//   int custom_algorithm(const int* data, int size, int target)
//
// Preconditions & Requirements:
//   Dataset Precondition: Ascending sorted integer array (required for Binary Search,
//   Jump Search, and Interpolation Search). Unsorted data may only be used with Linear Search.
//
// Parameters:
//   data   -> Pointer to the array of integers
//   size   -> Total number of elements in the array
//   target -> Value to search for
//
// Return:
//   0-based index of target if found (0 <= index < size)
//   -1 if target is not present in data
//
int custom_algorithm(const int* data, int size, int target) {
    // WRITE YOUR TARGETED SEARCH ALGORITHM HERE
    // Example: Linear Search, Binary Search, Jump Search, Interpolation Search
    for (int i = 0; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return -1;
}
