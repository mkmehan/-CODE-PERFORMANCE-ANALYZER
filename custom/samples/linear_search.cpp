#include <vector>
#include <cstddef>

// Standard Linear Search Algorithm
// Iterates through the array sequentially until the target element is found.
int linear_search(const std::vector<int>& data, int target) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == target) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

