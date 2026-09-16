#include <cstddef>

// ============================================================================
// Code Performance Analyzer - Matrix Operations (Extension Preview)
// Contract:
//   A -> Flat row-major n x n input matrix
//   B -> Flat row-major n x n input matrix
//   C -> Flat row-major n x n output matrix (pre-allocated)
//   n -> Matrix dimension (n x n)
// ============================================================================

void matrix_operation(const double* A, const double* B, double* C, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (int k = 0; k < n; ++k) {
                sum += A[i * n + k] * B[k * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

