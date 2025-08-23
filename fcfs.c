#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Structure to pass data to threads
typedef struct {
    int *p_name;
    int *burst_time;
    int *wt;
    int *tat;
    int start_idx;
    int end_idx;
    int n;
    int core_id;
    int processes_handled;
} ThreadData;

// Global variables for thread synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int *core_load; // Array to track how many processes each core handles

// Function to calculate waiting time for a range of processes
void *process_chunk(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    // Calculate waiting time
    for (int i = data->start_idx; i < data->end_idx; i++) {
        if (i == 0) {
            data->wt[0] = 0; // First process has 0 waiting time
        } else {
            data->wt[i] = data->wt[i-1] + data->burst_time[i-1];
        }
        
        // Calculate turnaround time
        data->tat[i] = data->burst_time[i] + data->wt[i];
        
        // Track that this core processed this process
        data->processes_handled++;
        
        // Simulate actual processing by sleeping briefly
        usleep(1000); // Sleep for 1ms to simulate work
        
        printf("Core %d processed P%d\n", data->core_id, data->p_name[i]);
    }
    
    // Update core load stats
    pthread_mutex_lock(&mutex);
    core_load[data->core_id] = data->processes_handled;
    pthread_mutex_unlock(&mutex);
    
    return NULL;
}

// Function to display results
void display_results(int p_name[], int burst_time[], int wt[], int tat[], int n) {
    printf("\n===== FCFS Scheduling Results =====\n");
    printf("Process\tBurst Time\tWaiting Time\tTurnaround Time\n");
    for (int i = 0; i < n; i++) {
        printf("P%d\t%d\t\t%d\t\t%d\n", p_name[i], burst_time[i], wt[i], tat[i]);
    }
    
    int total_wt = 0;
    int total_tat = 0;
    
    for (int i = 0; i < n; i++) {
        total_wt += wt[i];
        total_tat += tat[i];
    }
    
    float avg_wt = (float)total_wt / n;
    float avg_tat = (float)total_tat / n;
    
    printf("\nTotal waiting time: %d\n", total_wt);
    printf("Total turnaround time: %d\n", total_tat);
    printf("Average waiting time: %.2f\n", avg_wt);
    printf("Average turnaround time: %.2f\n", avg_tat);
}

// Function to display core utilization
void display_core_utilization(int num_cores, int n) {
    printf("\n===== Core Utilization =====\n");
    
    int total_processed = 0;
    for (int i = 0; i < num_cores; i++) {
        total_processed += core_load[i];
    }
    
    printf("Core ID\tProcesses\tUtilization\n");
    for (int i = 0; i < num_cores; i++) {
        float util_percent = (float)core_load[i] / n * 100;
        printf("%d\t%d\t\t%.2f%%\n", i, core_load[i], util_percent);
        
        // Visual representation of core utilization
        printf("        [");
        int bars = (int)(util_percent / 5);
        for (int j = 0; j < 20; j++) {
            if (j < bars) {
                printf("|");
            } else {
                printf(" ");
            }
        }
        printf("]\n");
    }
    
    printf("\nTotal processes: %d\n", n);
    printf("Total processes handled by all cores: %d\n", total_processed);
}

// Main parallel FCFS function
void parallel_fcfs(int p_name[], int burst_time[], int n, int num_cores) {
    // Limit number of cores to use (cannot exceed number of processes)
    if (num_cores > n) {
        printf("Notice: Number of cores reduced from %d to %d (equal to number of processes)\n", 
               num_cores, n);
        num_cores = n;
    }
    
    // Allocate memory for results
    int *wt = (int *)calloc(n, sizeof(int));
    int *tat = (int *)calloc(n, sizeof(int));
    
    // Allocate memory for core load tracking
    core_load = (int *)calloc(num_cores, sizeof(int));
    
    // Create thread data and threads
    pthread_t threads[num_cores];
    ThreadData thread_data[num_cores];
    
    // Calculate how many processes each thread will handle
    int chunk_size = n / num_cores;
    int remainder = n % num_cores;
    
    printf("\n===== Process Distribution =====\n");
    
    // Create and start threads
    for (int i = 0; i < num_cores; i++) {
        thread_data[i].p_name = p_name;
        thread_data[i].burst_time = burst_time;
        thread_data[i].wt = wt;
        thread_data[i].tat = tat;
        thread_data[i].n = n;
        thread_data[i].core_id = i;
        thread_data[i].processes_handled = 0;
        
        // Distribute processes evenly, handling remainder
        thread_data[i].start_idx = i * chunk_size + (i < remainder ? i : remainder);
        thread_data[i].end_idx = (i + 1) * chunk_size + (i + 1 < remainder ? i + 1 : remainder);
        
        printf("Core %d assigned processes: ", i);
        for (int j = thread_data[i].start_idx; j < thread_data[i].end_idx; j++) {
            printf("P%d ", p_name[j]);
        }
        printf("\n");
        
        pthread_create(&threads[i], NULL, process_chunk, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_cores; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Display results
    display_results(p_name, burst_time, wt, tat, n);
    display_core_utilization(num_cores, n);
    
    // Clean up
    free(wt);
    free(tat);
    free(core_load);
}

int main() {
    int n, num_cores;
    
    printf("Enter the number of processes in the ready queue: ");
    scanf("%d", &n);
    
    printf("Enter the number of cores to use for parallel execution: ");
    scanf("%d", &num_cores);
    
    if (num_cores <= 0) {
        printf("Error: Number of cores must be positive. Using 1 core.\n");
        num_cores = 1;
    }
    
    int *p_name = (int *)malloc(n * sizeof(int));
    int *burst_time = (int *)malloc(n * sizeof(int));
    
    printf("Enter the burst time for each process:\n");
    for (int i = 0; i < n; i++) {
        p_name[i] = i + 1;
        printf("P%d: ", i + 1);
        scanf("%d", &burst_time[i]);
    }
    
    printf("\nExecuting FCFS scheduling on %d cores...\n", num_cores);
    parallel_fcfs(p_name, burst_time, n, num_cores);
    
    free(p_name);
    free(burst_time);
    
    return 0;
}