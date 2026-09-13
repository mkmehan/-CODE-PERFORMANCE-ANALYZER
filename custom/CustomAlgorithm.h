#ifndef CUSTOM_ALGORITHM_H
#define CUSTOM_ALGORITHM_H

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace custom {

enum class CustomCategory {
    Search,
    Sorting,
    Matrix,
    Graph,
    Tree,
    Generic
};

inline std::string custom_category_to_string(CustomCategory cat) {
    switch (cat) {
        case CustomCategory::Search:   return "search";
        case CustomCategory::Sorting:  return "sorting";
        case CustomCategory::Matrix:   return "matrix";
        case CustomCategory::Graph:    return "graph";
        case CustomCategory::Tree:     return "tree";
        case CustomCategory::Generic:  return "generic";
    }
    return "unknown";
}

inline CustomCategory string_to_custom_category(const std::string& str) {
    if (str == "search") return CustomCategory::Search;
    if (str == "sorting") return CustomCategory::Sorting;
    if (str == "matrix") return CustomCategory::Matrix;
    if (str == "graph") return CustomCategory::Graph;
    if (str == "tree") return CustomCategory::Tree;
    return CustomCategory::Generic;
}

class ICustomAlgorithm {
public:
    virtual ~ICustomAlgorithm() = default;
    virtual std::string name() const = 0;
    virtual CustomCategory category() const = 0;
};

// Standard Search Algorithm Interface
class ISearchAlgorithm : public ICustomAlgorithm {
public:
    CustomCategory category() const override { return CustomCategory::Search; }

    // Optional preparation hook called before benchmark iterations (e.g. to pre-fill vector)
    virtual void prepare(const int* data, size_t size) {
        (void)data;
        (void)size;
    }

    // Standard search method: searches data[0..size-1] for target.
    // Returns 0-based index if found, or -1 if not found.
    virtual int search(const int* data, size_t size, int target) = 0;
};

} // namespace custom

#endif // CUSTOM_ALGORITHM_H

