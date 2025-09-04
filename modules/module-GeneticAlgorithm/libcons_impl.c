/*
 * Example implementation of libcons.so - Constraint Library
 * Compile with: gcc -shared -fPIC -o libcons.so libcons_impl.c -lm
 */

#include "libcons_interface.h"
#include <math.h>

// Example constraint bounds
static const double LOWER_BOUNDS[] = {0.0, 0.0, 0.0, 0.0, 0.0};  // Min values
static const double UPPER_BOUNDS[] = {1.0, 1.0, 1.0, 1.0, 1.0};  // Max values
static const int MAX_GENOME_LEN = 5;

int constraint_ok(const double* genome, int genomeLen) {
    // Check bounds constraints
    for (int i = 0; i < genomeLen && i < MAX_GENOME_LEN; i++) {
        if (genome[i] < LOWER_BOUNDS[i] || genome[i] > UPPER_BOUNDS[i]) {
            return 0;  // Constraint violated
        }
    }
    
    // Example additional constraint: sum of all parameters should be <= 3.0
    double sum = 0.0;
    for (int i = 0; i < genomeLen; i++) {
        sum += genome[i];
    }
    if (sum > 3.0) {
        return 0;  // Constraint violated
    }
    
    // Example nonlinear constraint: x1^2 + x2^2 <= 1.0 (if we have at least 2 parameters)
    if (genomeLen >= 2) {
        if (genome[0]*genome[0] + genome[1]*genome[1] > 1.0) {
            return 0;  // Constraint violated
        }
    }
    
    return 1;  // All constraints satisfied
}

double constraint_penalty(const double* genome, int genomeLen) {
    double penalty = 0.0;
    
    // Bounds penalty
    for (int i = 0; i < genomeLen && i < MAX_GENOME_LEN; i++) {
        if (genome[i] < LOWER_BOUNDS[i]) {
            penalty += (LOWER_BOUNDS[i] - genome[i]) * 1000.0;
        }
        if (genome[i] > UPPER_BOUNDS[i]) {
            penalty += (genome[i] - UPPER_BOUNDS[i]) * 1000.0;
        }
    }
    
    // Sum constraint penalty
    double sum = 0.0;
    for (int i = 0; i < genomeLen; i++) {
        sum += genome[i];
    }
    if (sum > 3.0) {
        penalty += (sum - 3.0) * 500.0;
    }
    
    // Nonlinear constraint penalty
    if (genomeLen >= 2) {
        double circle_constraint = genome[0]*genome[0] + genome[1]*genome[1];
        if (circle_constraint > 1.0) {
            penalty += (circle_constraint - 1.0) * 750.0;
        }
    }
    
    return penalty;
}

int check_bounds(const double* genome, const double* lower_bounds,
                 const double* upper_bounds, int genomeLen) {
    for (int i = 0; i < genomeLen; i++) {
        if (genome[i] < lower_bounds[i] || genome[i] > upper_bounds[i]) {
            return 0;  // Bounds violated
        }
    }
    return 1;  // All bounds satisfied
}

int constraint_details(const double* genome, int genomeLen,
                      int* violations, int* num_violations) {
    *num_violations = 0;
    int all_ok = 1;
    
    // Check bounds
    for (int i = 0; i < genomeLen && i < MAX_GENOME_LEN; i++) {
        violations[i] = 0;
        if (genome[i] < LOWER_BOUNDS[i] || genome[i] > UPPER_BOUNDS[i]) {
            violations[i] = 1;
            (*num_violations)++;
            all_ok = 0;
        }
    }
    
    // Check sum constraint (violation code 100)
    double sum = 0.0;
    for (int i = 0; i < genomeLen; i++) {
        sum += genome[i];
    }
    if (sum > 3.0 && genomeLen < MAX_GENOME_LEN) {
        violations[genomeLen] = 100;  // Special code for sum constraint
        (*num_violations)++;
        all_ok = 0;
    }
    
    // Check nonlinear constraint (violation code 200)
    if (genomeLen >= 2) {
        if (genome[0]*genome[0] + genome[1]*genome[1] > 1.0) {
            if (genomeLen + 1 < MAX_GENOME_LEN) {
                violations[genomeLen + 1] = 200;  // Special code for circle constraint
                (*num_violations)++;
                all_ok = 0;
            }
        }
    }
    
    return all_ok;
}
