/*
 * Minimal stub implementation of libcons.so for testing
 * constraint checking with the MBDyn GA optimization module
 */

#include <cmath>
#include <iostream>

extern "C" {

// Check if solution satisfies all constraints
bool constraint_check(const double* solution, int n_vars) {
    if (!solution || n_vars <= 0) {
        return false;
    }
    
    // Example constraints:
    // 1. All variables should be in range [-10, 10]
    // 2. Sum of variables should be less than 50
    
    double sum = 0.0;
    for (int i = 0; i < n_vars; ++i) {
        if (solution[i] < -10.0 || solution[i] > 10.0) {
            std::cout << "Constraint violation: var[" << i << "] = " << solution[i] 
                      << " outside [-10, 10]" << std::endl;
            return false;
        }
        sum += solution[i];
    }
    
    if (sum > 50.0) {
        std::cout << "Constraint violation: sum = " << sum << " > 50" << std::endl;
        return false;
    }
    
    return true;
}

// Calculate constraint violation measure (0 = feasible, >0 = infeasible)
double constraint_violation(const double* solution, int n_vars) {
    if (!solution || n_vars <= 0) {
        return 1000.0;  // Large penalty for invalid input
    }
    
    double violation = 0.0;
    double sum = 0.0;
    
    // Penalty for variables outside bounds
    for (int i = 0; i < n_vars; ++i) {
        if (solution[i] < -10.0) {
            violation += (-10.0 - solution[i]) * (-10.0 - solution[i]);
        } else if (solution[i] > 10.0) {
            violation += (solution[i] - 10.0) * (solution[i] - 10.0);
        }
        sum += solution[i];
    }
    
    // Penalty for sum constraint
    if (sum > 50.0) {
        violation += (sum - 50.0) * (sum - 50.0);
    }
    
    return violation;
}

// Initialize constraint checking system (optional)
int constraint_init() {
    std::cout << "Constraint system initialized" << std::endl;
    return 0;
}

// Clean up constraint checking system (optional)
void constraint_cleanup() {
    std::cout << "Constraint system cleaned up" << std::endl;
}

// Get number of constraints (optional, for information)
int get_num_constraints() {
    return 2;  // bounds + sum constraint
}

// Get constraint descriptions (optional, for debugging)
const char* get_constraint_description(int constraint_id) {
    switch (constraint_id) {
        case 0: return "Variable bounds: -10 <= x[i] <= 10";
        case 1: return "Sum constraint: sum(x[i]) <= 50";
        default: return "Unknown constraint";
    }
}

} // extern "C"