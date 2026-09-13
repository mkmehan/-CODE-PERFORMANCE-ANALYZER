#include <cstddef>

// Standard Binary Search Algorithm
// Performs O(log n) search on a pre-sorted array.
int binarySearch(const int* data, size_t size, int target) {
    int left = 0;
    int right = static_cast<int>(size) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (data[mid] == target) {
            return mid;
        }
        if (data[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

