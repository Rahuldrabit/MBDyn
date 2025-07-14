# MBDyn Genetic Algorithm Optimization Test

This example demonstrates how to integrate a Genetic Algorithm (GA) optimization routine into an MBDyn simulation using a custom user-defined element.

---

## 📄 Files Included

- `test_ga_superelement.mbd` – MBDyn input file with a user-defined GA block.
- `README.md` – This guide.

---

## ⚙️ Input File Structure Overview

```mbdyn
user defined: 1000, genetic algorithm optimization,

  inputs number, 5,
  element, 101, joint, string, "M[2]", direct,
  element, 102, joint, string, "M[2]", direct,
  element, 103, joint, string, "M[2]", direct,
  element, 104, joint, string, "M[2]", direct,
  element, 105, joint, string, "M[2]", direct,

  output number, 3,
  121,
  123,
  126,

  # GA Parameters
  crossover type, string, "arithmetic",
  mutation type, string, "gaussian",
  selection type, string, "tournament",

  popSize,   20,
  numGen,    1,
  crossRate, 0.9,
  mutRate,   0.1,
  elitism,   2,
  history,   10,
  seed,      42;
```

---

## 🧬 GA Configuration Parameters

| Parameter       | Description                                                                 |
|----------------|-----------------------------------------------------------------------------|
| `popSize`       | Size of the population (number of individuals per generation)              |
| `numGen`        | Number of generations per optimization call (often set to 1 in real-time)  |
| `crossRate`     | Crossover probability (typically between 0.6–0.9)                           |
| `mutRate`       | Mutation probability per gene (e.g., 0.01–0.2)                              |
| `elitism`       | Number of best individuals preserved each generation                        |
| `history`       | Number of past generations to keep track of (optional, for analysis/logs)   |
| `seed`          | Seed for the random number generator (ensures reproducibility)              |

---

## 🔁 GA Operators

| Operator            | Options        | Explanation                                                                 |
|---------------------|----------------|-----------------------------------------------------------------------------|
| `crossover type`     | `uniform`, `arithmetic`, `single-point`, `two-point` | Defines how parent individuals combine their genes                         |
| `mutation type`      | `gaussian`, `uniform`, `creep`                      | Determines how random variation is introduced                              |
| `selection type`     | `tournament`, `roulette`, `rank`                   | Determines how individuals are selected for reproduction                    |

---

## 🔌 Inputs and Outputs

- `inputs number`: Specifies how many actuator or muscle elements are controlled.
  - Each `element, <ID>, joint, string, "M[2]", direct` line maps to one element and its control mode.
  - `"M[2]"` might refer to a moment or torque DOF.

- `output number`: IDs of `drive caller` outputs to observe GA result (e.g., activation, torque, etc.)

---

## 🛠 Notes

- You must implement the corresponding `GeneticAlgorithmOptimization` logic in C++.
- This block should:
  - Read element states
  - Evaluate fitness using `libgafitness.so` or inline logic
  - Apply optimized activation to the connected elements
- `.so` files (e.g. `libgafitness.so`) can be dynamically linked to compute fitness or constraints externally.

---

## ✅ Running the Simulation

1. Build your user-defined element plugin.
2. Run MBDyn with this input file:
   ```bash
   mbdyn -f test_ga_superelement.mbd
   ```
3. Analyze results in `output.res` or via the configured output channels.

---

Need help writing the C++ UDE block or `.so` fitness function? Let me know!