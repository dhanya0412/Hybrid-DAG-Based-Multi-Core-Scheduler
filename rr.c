#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct {
    int id;
    char name[20];
    int burst_time;
    int remaining_time;
    int waiting_time;
    int turnaround_time;
    int completion_time;
    int arrival_time;  // When the process becomes available
    bool is_completed;
    int core_assigned;  // Track which core is running this process
} Process;

typedef struct {
    int core_id;
    Process* current_process;  // Currently running process
    int time_slice_remaining;  // Remaining quantum time
    bool is_idle;             // Whether core is currently idle
    int total_idle_time;      // Statistics: total time spent idle
} Core;

// Function to introduce a short delay for visualization purposes
void delay_ms(int ms) {
    // Only delay if value is reasonable (avoid very long pauses)
    if (ms > 0 && ms <= 100) {
        usleep(ms * 1000);  // Convert milliseconds to microseconds
    }
}

// Function to print progress bar
void print_progress_bar(int progress, int total) {
    const int bar_width = 50;
    float completion_rate = (float)progress / total;
    int filled_positions = bar_width * completion_rate;
    
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled_positions) printf("=");
        else if (i == filled_positions) printf(">");
        else printf(" ");
    }
    printf("] %d%%\r", (int)(completion_rate * 100));
    fflush(stdout);
}

// Function to initialize core structures
Core* init_cores(int num_cores) {
    Core* cores = (Core*)malloc(num_cores * sizeof(Core));
    for (int i = 0; i < num_cores; i++) {
        cores[i].core_id = i;
        cores[i].current_process = NULL;
        cores[i].time_slice_remaining = 0;
        cores[i].is_idle = true;
        cores[i].total_idle_time = 0;
    }
    return cores;
}

// Function to find next process to execute in round-robin manner
int find_next_ready_process(Process processes[], int n, int last_idx) {
    int next_idx = (last_idx + 1) % n;
    int count = 0;
    
    // Find next non-completed process that has arrived
    while ((processes[next_idx].is_completed || 
            processes[next_idx].arrival_time > 0) && count < n) {
        next_idx = (next_idx + 1) % n;
        count++;
    }
    
    if (count == n) {
        // No ready processes found
        return -1;
    }
    
    return next_idx;
}

// Multi-core round robin scheduling algorithm
void multi_core_round_robin(Process processes[], int n, int num_cores, int time_quantum, bool debug_mode) {
    // Initialize cores
    Core* cores = init_cores(num_cores);
    
    int current_time = 0;
    int completed_processes = 0;
    int last_scheduled_idx = -1;
    
    // Continue until all processes complete
    while (completed_processes < n) {
        // Update arrival times
        for (int i = 0; i < n; i++) {
            if (processes[i].arrival_time > 0) {
                processes[i].arrival_time--;
            }
        }
        
        // Process currently running tasks on each core
        for (int core_idx = 0; core_idx < num_cores; core_idx++) {
            if (!cores[core_idx].is_idle) {
                Process* active_process = cores[core_idx].current_process;
                
                // Process one time unit on this core
                active_process->remaining_time--;
                cores[core_idx].time_slice_remaining--;
                
                // Check if process has completed
                if (active_process->remaining_time <= 0) {
                    // Mark process as completed
                    active_process->is_completed = true;
                    active_process->core_assigned = -1;
                    active_process->completion_time = current_time;
                    completed_processes++;
                    
                    if (debug_mode) {
                        printf("Time %d: Core %d completed process %s\n", 
                               current_time, core_idx, active_process->name);
                    }
                    
                    // Free the core
                    cores[core_idx].is_idle = true;
                    cores[core_idx].current_process = NULL;
                    cores[core_idx].time_slice_remaining = 0;
                }
                // Check if time quantum has expired
                else if (cores[core_idx].time_slice_remaining <= 0) {
                    if (debug_mode) {
                        printf("Time %d: Core %d preempted process %s (remaining: %d)\n", 
                               current_time, core_idx, active_process->name, active_process->remaining_time);
                    }
                    
                    // Return process to ready queue
                    active_process->core_assigned = -1;
                    cores[core_idx].is_idle = true;
                    cores[core_idx].current_process = NULL;
                }
            }
        }
        
        // Assign processes to idle cores in round-robin fashion
        for (int core_idx = 0; core_idx < num_cores && completed_processes < n; core_idx++) {
            if (cores[core_idx].is_idle) {
                // Find a process that's ready, not completed, and not assigned to a core
                bool found_process = false;
                int checked_count = 0;
                
                while (checked_count < n && !found_process) {
                    last_scheduled_idx = (last_scheduled_idx + 1) % n;
                    
                    // Check if this process is available to run
                    if (!processes[last_scheduled_idx].is_completed && 
                        processes[last_scheduled_idx].core_assigned == -1 &&
                        processes[last_scheduled_idx].arrival_time <= 0) {
                        found_process = true;
                    } else {
                        checked_count++;
                    }
                }
                
                // If we found a process to run
                if (found_process) {
                    Process* next_process = &processes[last_scheduled_idx];
                    
                    // Assign to core
                    cores[core_idx].current_process = next_process;
                    cores[core_idx].is_idle = false;
                    cores[core_idx].time_slice_remaining = time_quantum;
                    
                    next_process->core_assigned = core_idx;
                    
                    if (debug_mode) {
                        printf("Time %d: Core %d started process %s (remaining: %d)\n", 
                               current_time, core_idx, next_process->name, next_process->remaining_time);
                    }
                }
            }
        }
        
        // Track waiting time for processes that are ready but not currently running
        for (int i = 0; i < n; i++) {
            if (!processes[i].is_completed && 
                processes[i].core_assigned == -1 && 
                processes[i].arrival_time <= 0) {
                processes[i].waiting_time++;
            }
        }
        
        // Advance time
        current_time++;
        
        // Track idle time for cores
        for (int core_idx = 0; core_idx < num_cores; core_idx++) {
            if (cores[core_idx].is_idle) {
                cores[core_idx].total_idle_time++;
            }
        }
        
        // Show progress periodically
        if (current_time % 20 == 0) {
            print_progress_bar(completed_processes, n);
        }
        
        // Small delay for visualization
        delay_ms(10);
        
        // Safety check to prevent infinite loops
        if (current_time > 10000) {
            printf("\nSimulation exceeded time limit. Exiting.\n");
            break;
        }
    }
    
    // Calculate turnaround time for each process
    for (int i = 0; i < n; i++) {
        processes[i].turnaround_time = processes[i].completion_time;  // No arrival offset
    }
    
    // Print results
    printf("\n===== Multi-Core Round Robin Results =====\n");
    printf("Process    | Burst Time | Completion | Waiting | Turnaround\n");
    printf("--------------------------------------------------------\n");
    
    int total_waiting = 0;
    int total_turnaround = 0;
    
    for (int i = 0; i < n; i++) {
        printf("%-10s | %-10d | %-10d | %-7d | %-10d\n",
               processes[i].name, processes[i].burst_time,
               processes[i].completion_time, processes[i].waiting_time,
               processes[i].turnaround_time);
        
        total_waiting += processes[i].waiting_time;
        total_turnaround += processes[i].turnaround_time;
    }
    
    // Print average times
    printf("\nAverage Waiting Time: %.2f\n", (float)total_waiting / n);
    printf("Average Turnaround Time: %.2f\n", (float)total_turnaround / n);
    
    // Print core utilization statistics
    printf("\n===== Core Utilization Statistics =====\n");
    printf("Core | Busy Time | Idle Time | Utilization %%\n");
    printf("----------------------------------------\n");
    
    float total_utilization = 0.0;
    for (int core_idx = 0; core_idx < num_cores; core_idx++) {
        int busy_time = current_time - cores[core_idx].total_idle_time;
        float utilization_percent = (float)busy_time / current_time * 100.0;
        total_utilization += utilization_percent;
        
        printf("%-4d | %-9d | %-9d | %.2f%%\n",
               core_idx, busy_time, cores[core_idx].total_idle_time, utilization_percent);
    }
    
    // Print average core utilization
    printf("\nAverage Core Utilization: %.2f%%\n", total_utilization / num_cores);
    
    // Free allocated memory
    free(cores);
}

// Create sample processes matching the sample DAG in scheduler.c
Process* create_sample_processes(int* num_processes) {
    // Same execution times as in the sample DAG
    int execution_times[10] = {172, 105, 252, 91, 120, 138, 47, 65, 185, 78};
    
    // Create and initialize processes
    *num_processes = 10;
    Process* processes = (Process*)malloc(*num_processes * sizeof(Process));
    
    for (int i = 0; i < *num_processes; i++) {
        processes[i].id = i;
        sprintf(processes[i].name, "Task%d", i);
        processes[i].burst_time = execution_times[i];
        processes[i].remaining_time = execution_times[i];
        processes[i].waiting_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].completion_time = 0;
        processes[i].arrival_time = 0;  // All available at start for simple comparison
        processes[i].is_completed = false;
        processes[i].core_assigned = -1;
    }
    
    return processes;
}

// Create custom processes based on user input
Process* create_custom_processes(int* num_processes) {
    printf("Enter the number of processes (1-100): ");
    scanf("%d", num_processes);
    
    if (*num_processes < 1 || *num_processes > 100) {
        printf("Invalid number. Using 5 processes.\n");
        *num_processes = 5;
    }
    
    Process* processes = (Process*)malloc(*num_processes * sizeof(Process));
    
    for (int i = 0; i < *num_processes; i++) {
        processes[i].id = i;
        sprintf(processes[i].name, "P%d", i+1);
        
        printf("Enter burst time for process %s: ", processes[i].name);
        scanf("%d", &processes[i].burst_time);
        processes[i].remaining_time = processes[i].burst_time;
        
        processes[i].waiting_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].completion_time = 0;
        processes[i].is_completed = false;
        processes[i].core_assigned = -1;
        
        printf("Enter arrival time for process %s: ", processes[i].name);
        scanf("%d", &processes[i].arrival_time);
    }
    
    return processes;
}

// Main function
int main() {
    int choice, num_processes = 0, time_quantum, num_cores;
    Process* processes = NULL;
    bool debug_mode = false;
    
    printf("\nMulti-Core Round Robin Scheduler\n");
    printf("===============================\n");
    printf("1. Use Sample Processes (matches sample DAG)\n");
    printf("2. Create Custom Processes\n");
    printf("Enter your choice: ");
    scanf("%d", &choice);
    
    // Create processes based on choice
    if (choice == 1) {
        processes = create_sample_processes(&num_processes);
        printf("Created %d sample processes matching the DAG example\n", num_processes);
    } else {
        processes = create_custom_processes(&num_processes);
    }
    
    // Get scheduler parameters
    printf("Enter time quantum: ");
    scanf("%d", &time_quantum);
    
    printf("Enter number of CPU cores (1-16): ");
    scanf("%d", &num_cores);
    if (num_cores < 1 || num_cores > 16) {
        printf("Invalid number of cores. Using 4 cores.\n");
        num_cores = 4;
    }
    
    printf("Enable debug mode? (0-No, 1-Yes): ");
    scanf("%d", (int*)&debug_mode);
    
    // Run the scheduler
    printf("\nRunning Multi-Core Round Robin scheduling with %d cores and time quantum %d...\n", 
           num_cores, time_quantum);
    multi_core_round_robin(processes, num_processes, num_cores, time_quantum, debug_mode);
    
    // Free allocated memory
    free(processes);
    
    return 0;
}