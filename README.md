# Hybrid-DAG-Based-Multi-Core-Scheduler
A high-performance scheduler that combines **DAG-aware dependency handling** with **Rate Monotonic Scheduling (RMS)** priorities to run dependent tasks across **multiple cores** using **POSIX Threads (Pthreads)**.

> Traditional FCFS/RR schedulers struggle with dependencies. This hybrid design respects the DAG while keeping high-frequency tasks responsive via RMS, and reports **turnaround time** and **core utilization** 
---
## Project Summary
This project implements a **hybrid scheduler** which:
- Models tasks and dependencies as a **Directed Acyclic Graph (DAG)**.
- Uses **Rate Monotonic Scheduling (RMS)** to assign fixed priorities (shorter period → higher priority).
- Simulates preemptive multi-core execution using **POSIX threads (Pthreads)** and time slices.
- Produces per-task and per-core metrics and can export results to CSV for further analysis.

It is implemented in **C** (single-file `scheduler.c` in this repo) and uses standard POSIX APIs.

---

## Features
- DAG-aware scheduling with **dependency checks** and **cycle detection** (DFS + recursion stack).
- **RMS priority assignment** (period → priority mapping, clamped to `[1,10]`).
- Preemptive, multi-core simulation using **Pthreads** semantics.
- Time-sliced execution; configurable quantum.
- Safety cap for runaway simulations (`simulation_time > 10000` time units).
- Console trace/debug mode and **CSV export**:
  - `scheduler_results_<name>_<N>_cores.csv`
  - `core_utilization_<name>_<N>_cores.csv`
- Interactive CLI: create sample DAG or custom DAG, run simulations, export results.

---

## Tech Stack
- Language: **C**
- Concurrency: **POSIX Threads (Pthreads)**
- Platform: **Linux, macOS, WSL2 (Ubuntu on Windows)**

---

## How to Use (Menu Driven)

When you run the program:

```bash
gcc scheduler.c -o scheduler -pthread
./scheduler
```

1. Create Sample DAG

Builds the sample 10-task DAG used in the project report.

2. Create Custom DAG

Interactive DAG creation:

Enter the number of tasks (num_tasks).

For each task, provide its duration (ms) and period (ms).

Enter dependency pairs in the form:

<taskID> <dependencyID>


Enter -1 when finished.

3. Display Current DAG

Prints the task list, assigned priorities, and the adjacency matrix.

4. Run Performance Comparison

Runs the Hybrid DAG + RMS scheduler simulation.

You will be prompted to:

Enter number of cores (1–16).

Enter time quantum (ms) → must be ≥ 10 ms (default: 50 ms).

Toggle debug mode to see detailed logs (Start / Preempt / Complete events).

5. Export Results to CSV

Writes two CSV files:

scheduler_results_<name>_<num_cores>_cores.csv (per-task results).

core_utilization_<name>_<num_cores>_cores.csv (per-core utilization stats).

6. Exit

Quits the program.

⚙️ Defaults & Constraints

MAX_TASKS = 100

MAX_CORES = 16

MIN_QUANTUM = 10 ms

DEFAULT_QUANTUM = 50 ms

Safety cap: simulation stops if simulation_time > 10000.

