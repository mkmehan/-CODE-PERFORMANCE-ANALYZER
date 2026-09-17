#include <cstddef>

// Code Performance Analyzer - Custom In-Place Sorting Contract
//
// Required Function Contract:
//   void custom_algorithm(int* data, int size)
//
// Parameters:
//   data -> Pointer to array of integer elements to sort in-place
//   size -> Number of elements in data
//
// Return:
//   void (data must be sorted in non-decreasing order upon return)
//
void custom_algorithm(int* data, int size) {
    // WRITE YOUR SORTING ALGORITHM HERE
    // Example: Bubble Sort, Insertion Sort, Selection Sort, Quick Sort, Merge Sort
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
