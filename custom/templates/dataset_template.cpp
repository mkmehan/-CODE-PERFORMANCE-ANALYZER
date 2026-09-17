#include <vector>

// Code Performance Analyzer - Full Dataset Operation Contract
//
// Required Function Contract:
//   long long custom_algorithm(const int* data, int size)
//
// Parameters:
//   data   -> Pointer to the array of integers
//   size   -> Total number of elements in the array
//
// Return:
//   Computed index, aggregate sum, max/min, or 0 on success
//
long long custom_algorithm(const int* data, int size) {
    // WRITE YOUR DATASET OPERATION ALGORITHM HERE
    // Example: Max/Min Search, Sum/Reduction, Count, BST Build/Traversal
    long long max_val = -2147483647 - 1;
    int max_idx = -1;
    for (int i = 0; i < size; ++i) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = i;
        }
    }
    return max_idx;
}

