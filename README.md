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

