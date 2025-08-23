# Hybrid-DAG-Based-Multi-Core-Scheduler
A high-performance scheduler that combines **DAG-aware dependency handling** with **Rate Monotonic Scheduling (RMS)** priorities to run dependent tasks across **multiple cores** using **POSIX Threads (Pthreads)**.

> Why it matters: traditional FCFS/RR schedulers struggle with dependencies. This hybrid design respects the DAG while keeping high-frequency tasks responsive via RMS, and reports **turnaround time** and **core utilization** out-of-the-box. :contentReference[oaicite:0]{index=0}

---
