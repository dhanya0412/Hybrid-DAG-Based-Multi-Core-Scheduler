#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_TASKS 100
#define MAX_CORES 16
#define MIN_QUANTUM 10
#define DEFAULT_QUANTUM 50

typedef struct {
    int id;
    char name[20];
    int duration; // in milliseconds
    int period;   // period for RMS (in milliseconds) - added for RMS
    int priority; // will be calculated based on period (RMS)
    int dependencies[MAX_TASKS];
    int dep_count;
    bool completed;
    int remaining_time;
    int core_assigned;
    int start_time;
    int finish_time;
} Task;

typedef struct {
    Task* tasks;
    int num_tasks;
    int** adjacency_matrix;
    bool has_cycles;
} DAG;

typedef struct {
    int core_id;
    Task* current_task;
    int time_slice_remaining;
    bool is_idle;
    int total_idle_time;
} Core;

// Global variables
DAG* current_dag = NULL;
Core* cores = NULL;
int simulation_time = 0;
int completed_tasks = 0;
int quantum = DEFAULT_QUANTUM;
bool debug_mode = false;

// Function prototypes
DAG* create_sample_dag();
DAG* create_custom_dag();
void display_dag(DAG* dag);
void run_performance_comparison(int num_cores);
void export_results_to_csv(DAG* dag, char* scheduler_name, int num_cores);
void free_dag(DAG* dag);
void detect_cycles(DAG* dag);
bool is_task_ready(DAG* dag, int task_id);
void reset_dag_execution(DAG* dag);
void simulate_hybrid_scheduler(DAG* dag, int num_cores);
int find_highest_priority_ready_task(DAG* dag);
void print_execution_trace(int time, int core_id, Task* task, const char* event);
void print_progress_bar(int progress, int total);
void apply_rate_monotonic_scheduling(DAG* dag); // New function for RMS

void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void delay_ms(int ms) {
    // Simulated delay for visualization
    if (ms > 0 && ms <= 100) {
        usleep(ms * 1000);  // Convert ms to microseconds
    }
}

// New function to apply Rate Monotonic Scheduling priority assignment
void apply_rate_monotonic_scheduling(DAG* dag) {
    // Sort tasks by period (shortest period gets highest priority)
    // In RMS, priority is inversely proportional to period
    for (int i = 0; i < dag->num_tasks; i++) {
        if (dag->tasks[i].period == 0) {
            // Non-periodic tasks get lowest priority
            dag->tasks[i].priority = 1;
        } else {
            // Calculate priority: smaller periods get higher priority values
            // Scale to 1-10 range for compatibility with existing code
            // Lower period = higher priority number
            dag->tasks[i].priority = 10 - ((dag->tasks[i].period * 9) / 1000);
            
            // Ensure priority is within bounds 1-10
            if (dag->tasks[i].priority < 1) dag->tasks[i].priority = 1;
            if (dag->tasks[i].priority > 10) dag->tasks[i].priority = 10;
        }
        
        if (debug_mode) {
            printf("Task %d (%s): Period=%d, Assigned Priority=%d\n", 
                  dag->tasks[i].id, dag->tasks[i].name, 
                  dag->tasks[i].period, dag->tasks[i].priority);
        }
    }
}

DAG* create_dag(int num_tasks) {
    DAG* dag = (DAG*)malloc(sizeof(DAG));
    if (!dag) {
        printf("Memory allocation failed for DAG\n");
        exit(1);
    }
    
    dag->num_tasks = num_tasks;
    dag->has_cycles = false;
    
    // Allocate tasks
    dag->tasks = (Task*)malloc(num_tasks * sizeof(Task));
    if (!dag->tasks) {
        printf("Memory allocation failed for tasks\n");
        free(dag);
        exit(1);
    }
    
    // Allocate adjacency matrix
    dag->adjacency_matrix = (int**)malloc(num_tasks * sizeof(int*));
    if (!dag->adjacency_matrix) {
        printf("Memory allocation failed for adjacency matrix\n");
        free(dag->tasks);
        free(dag);
        exit(1);
    }
    
    for (int i = 0; i < num_tasks; i++) {
        dag->adjacency_matrix[i] = (int*)calloc(num_tasks, sizeof(int));
        if (!dag->adjacency_matrix[i]) {
            printf("Memory allocation failed for adjacency matrix row\n");
            for (int j = 0; j < i; j++) {
                free(dag->adjacency_matrix[j]);
            }
            free(dag->adjacency_matrix);
            free(dag->tasks);
            free(dag);
            exit(1);
        }
    }
    
    // Initialize tasks
    for (int i = 0; i < num_tasks; i++) {
        dag->tasks[i].id = i;
        sprintf(dag->tasks[i].name, "Task%d", i);
        dag->tasks[i].dep_count = 0;
        dag->tasks[i].completed = false;
        dag->tasks[i].core_assigned = -1;
        dag->tasks[i].start_time = -1;
        dag->tasks[i].finish_time = -1;
    }
    
    return dag;
}

DAG* create_sample_dag() {
    int num_tasks = 10;
    DAG* dag = create_dag(num_tasks);
    
    // Define task durations and periods (for RMS)
    int durations[10] = {172, 105, 252, 91, 120, 138, 47, 65, 185, 78};
    int periods[10] = {500, 200, 800, 300, 250, 350, 150, 400, 600, 100}; // Added periods for RMS
    
    for (int i = 0; i < num_tasks; i++) {
        dag->tasks[i].duration = durations[i];
        dag->tasks[i].remaining_time = durations[i];
        dag->tasks[i].period = periods[i]; // Set the period
    }
    
    // Apply RMS to set priorities based on periods
    apply_rate_monotonic_scheduling(dag);
    
    // Define dependencies (task i depends on task j)
    int dependencies[][2] = {
        {1, 0}, // Task 1 depends on Task 0
        {2, 0}, // Task 2 depends on Task 0
        {3, 1}, // Task 3 depends on Task 1
        {4, 1}, // Task 4 depends on Task 1
        {5, 2}, // Task 5 depends on Task 2
        {6, 3}, // Task 6 depends on Task 3
        {6, 4}, // Task 6 also depends on Task 4
        {7, 5}, // Task 7 depends on Task 5
        {8, 6}, // Task 8 depends on Task 6
        {9, 7}, // Task 9 depends on Task 7
        {9, 8}, // Task 9 also depends on Task 8
    };
    
    int num_deps = sizeof(dependencies) / sizeof(dependencies[0]);
    
    // Fill adjacency matrix and dependency lists
    for (int i = 0; i < num_deps; i++) {
        int task = dependencies[i][0];
        int depends_on = dependencies[i][1];
        
        // FIXED: Proper representation in adjacency matrix
        // If task depends on depends_on, then there's an edge from depends_on to task
        dag->adjacency_matrix[depends_on][task] = 1; // Edge from depends_on to task
        
        // Add to dependency list
        dag->tasks[task].dependencies[dag->tasks[task].dep_count++] = depends_on;
    }
    
    // Check for cycles
    detect_cycles(dag);
    
    printf("Sample DAG created with %d tasks using Rate Monotonic Scheduling\n", num_tasks);
    return dag;
}

DAG* create_custom_dag() {
    int num_tasks;
    printf("Enter number of tasks (1-%d): ", MAX_TASKS);
    scanf("%d", &num_tasks);
    
    if (num_tasks < 1 || num_tasks > MAX_TASKS) {
        printf("Invalid number of tasks. Using 5 tasks.\n");
        num_tasks = 5;
    }
    
    DAG* dag = create_dag(num_tasks);
    
    for (int i = 0; i < num_tasks; i++) {
        printf("\nTask %d:\n", i);
        
        printf("Enter execution time (ms): ");
        scanf("%d", &dag->tasks[i].duration);
        dag->tasks[i].remaining_time = dag->tasks[i].duration;
        
        printf("Enter period (ms, lower period = higher priority, 0 for non-periodic): ");
        scanf("%d", &dag->tasks[i].period);
        
        if (dag->tasks[i].period < 0) {  // Changed from < 1 to < 0
            printf("Invalid period. Using default (500 ms).\n");
            dag->tasks[i].period = 500;
        }
    }
    
    // Apply RMS to set priorities based on periods
    apply_rate_monotonic_scheduling(dag);
    
    // Define dependencies
    printf("\nDefine dependencies (enter -1 to stop):\n");
    while (true) {
        int task, depends_on;
        
        printf("Enter task ID: ");
        scanf("%d", &task);
        if (task == -1) break;
        
        if (task < 0 || task >= num_tasks) {
            printf("Invalid task ID\n");
            continue;
        }
        
        printf("Depends on task ID: ");
        scanf("%d", &depends_on);
        
        if (depends_on < 0 || depends_on >= num_tasks) {
            printf("Invalid dependency task ID\n");
            continue;
        }
        
        if (task == depends_on) {
            printf("Task cannot depend on itself\n");
            continue;
        }
        
        // Check if dependency already exists
        bool exists = false;
        for (int i = 0; i < dag->tasks[task].dep_count; i++) {
            if (dag->tasks[task].dependencies[i] == depends_on) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            // FIXED: Proper representation in adjacency matrix
            // If task depends on depends_on, then there's an edge from depends_on to task
            dag->adjacency_matrix[depends_on][task] = 1;
            dag->tasks[task].dependencies[dag->tasks[task].dep_count++] = depends_on;
            printf("Added: Task %d depends on Task %d\n", task, depends_on);
        } else {
            printf("Dependency already exists\n");
        }
    }
    
    // Check for cycles
    detect_cycles(dag);
    
    return dag;
}

void dfs_cycle_detection(DAG* dag, int node, bool* visited, bool* rec_stack, bool* has_cycle) {
    if (*has_cycle) return; // Already found a cycle
    
    visited[node] = true;
    rec_stack[node] = true;
    
    for (int i = 0; i < dag->num_tasks; i++) {
        // If there's an edge from node to i
        if (dag->adjacency_matrix[node][i]) {
            if (!visited[i]) {
                dfs_cycle_detection(dag, i, visited, rec_stack, has_cycle);
                if (*has_cycle) return;
            } else if (rec_stack[i]) {
                *has_cycle = true;
                return;
            }
        }
    }
    
    rec_stack[node] = false;
}

void detect_cycles(DAG* dag) {
    bool *visited = (bool*)calloc(dag->num_tasks, sizeof(bool));
    bool *rec_stack = (bool*)calloc(dag->num_tasks, sizeof(bool));
    bool has_cycle = false;
    
    for (int i = 0; i < dag->num_tasks; i++) {
        if (!visited[i]) {
            dfs_cycle_detection(dag, i, visited, rec_stack, &has_cycle);
        }
    }
    
    dag->has_cycles = has_cycle;
    
    free(visited);
    free(rec_stack);
    
    if (has_cycle) {
        printf("WARNING: Cycles detected in the DAG! This may cause scheduler issues.\n");
    }
}

void display_dag(DAG* dag) {
    if (!dag) {
        printf("No DAG available. Please create one first.\n");
        return;
    }
    
    printf("\n===== DAG Information =====\n");
    printf("Number of tasks: %d\n", dag->num_tasks);
    printf("Has cycles: %s\n", dag->has_cycles ? "Yes (WARNING)" : "No");
    
    printf("\nTask Details (using Rate Monotonic Scheduling):\n");
    printf("ID | Name       | Duration | Period  | Priority | Dependencies\n");
    printf("----------------------------------------------------------\n");
    
    for (int i = 0; i < dag->num_tasks; i++) {
        printf("%-2d | %-10s | %-8d | %-7d | %-8d | ", 
               dag->tasks[i].id, 
               dag->tasks[i].name, 
               dag->tasks[i].duration, 
               dag->tasks[i].period,
               dag->tasks[i].priority);
        
        if (dag->tasks[i].dep_count == 0) {
            printf("None");
        } else {
            for (int j = 0; j < dag->tasks[i].dep_count; j++) {
                printf("%d ", dag->tasks[i].dependencies[j]);
            }
        }
        printf("\n");
    }
    
    // FIXED: Updated explanation of adjacency matrix
    printf("\nAdjacency Matrix (1 means row points to column, i.e., column depends on row):\n");
    printf("   ");
    for (int i = 0; i < dag->num_tasks; i++) {
        printf("%2d ", i);
    }
    printf("\n");
    
    for (int i = 0; i < dag->num_tasks; i++) {
        printf("%2d ", i);
        for (int j = 0; j < dag->num_tasks; j++) {
            printf("%2d ", dag->adjacency_matrix[i][j]);
        }
        printf("\n");
    }
}

bool is_task_ready(DAG* dag, int task_id) {
    if (dag->tasks[task_id].completed) {
        return false;
    }
    
    // Check if all dependencies are completed
    for (int i = 0; i < dag->tasks[task_id].dep_count; i++) {
        int dep_id = dag->tasks[task_id].dependencies[i];
        if (!dag->tasks[dep_id].completed) {
            return false;
        }
    }
    
    return true;
}

int find_highest_priority_ready_task(DAG* dag) {
    int highest_priority = -1;
    int selected_task = -1;
    
    for (int i = 0; i < dag->num_tasks; i++) {
        if (is_task_ready(dag, i) && dag->tasks[i].core_assigned == -1) {
            // RMS: Higher priority value means higher priority
            if (selected_task == -1 || dag->tasks[i].priority > highest_priority) {
                highest_priority = dag->tasks[i].priority;
                selected_task = i;
            }
            // If priorities are equal, choose the task with shorter period
            else if (dag->tasks[i].priority == highest_priority &&
                     dag->tasks[i].period < dag->tasks[selected_task].period) {
                selected_task = i;
            }
        }
    }
    
    return selected_task;
}

void reset_dag_execution(DAG* dag) {
    for (int i = 0; i < dag->num_tasks; i++) {
        dag->tasks[i].completed = false;
        dag->tasks[i].remaining_time = dag->tasks[i].duration;
        dag->tasks[i].core_assigned = -1;
        dag->tasks[i].start_time = -1;
        dag->tasks[i].finish_time = -1;
    }
}

void print_execution_trace(int time, int core_id, Task* task, const char* event) {
    if (debug_mode) {
        printf("Time %d: Core %d - %s %s (Period: %d, Priority: %d, %d ms remaining)\n", 
               time, core_id, event, task->name, 
               task->period, task->priority, task->remaining_time);
    }
}

void print_progress_bar(int progress, int total) {
    const int bar_width = 50;
    float percentage = (float)progress / total;
    int pos = bar_width * percentage;
    
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d%%\r", (int)(percentage * 100));
    fflush(stdout);
}

void simulate_hybrid_scheduler(DAG* dag, int num_cores) {
    printf("Running Hybrid DAG-based Scheduler with Rate Monotonic Scheduling...\n");
    
    // Initialize
    reset_dag_execution(dag);
    simulation_time = 0;
    completed_tasks = 0;
    
    // Allocate and initialize cores
    cores = (Core*)malloc(num_cores * sizeof(Core));
    for (int i = 0; i < num_cores; i++) {
        cores[i].core_id = i;
        cores[i].current_task = NULL;
        cores[i].time_slice_remaining = 0;
        cores[i].is_idle = true;
        cores[i].total_idle_time = 0;  // Initialize idle time counter
    }
    
    // Main simulation loop
    while (completed_tasks < dag->num_tasks) {
        // Check for completed tasks
        for (int i = 0; i < num_cores; i++) {
            if (!cores[i].is_idle) {
                Task* task = cores[i].current_task;
                
                // Reduce remaining time
                task->remaining_time--;
                cores[i].time_slice_remaining--;
                
                // Task completed
                if (task->remaining_time <= 0) {
                    task->completed = true;
                    task->core_assigned = -1;
                    task->finish_time = simulation_time;
                    completed_tasks++;
                    
                    print_execution_trace(simulation_time, i, task, "Completed");
                    printf("Completed Task %d (%s) on Core %d for %d ms (Period: %d ms, Priority: %d)\n", 
                           task->id, task->name, i, task->duration, task->period, task->priority);
                    
                    cores[i].is_idle = true;
                    cores[i].current_task = NULL;
                    cores[i].time_slice_remaining = 0;
                }
                // Time slice expired
                else if (cores[i].time_slice_remaining <= 0) {
                    print_execution_trace(simulation_time, i, task, "Preempted");
                    
                    // Put task back into ready queue
                    task->core_assigned = -1;
                    cores[i].is_idle = true;
                    cores[i].current_task = NULL;
                }
            }
        }
        
        // Assign tasks to idle cores
        for (int i = 0; i < num_cores; i++) {
            if (cores[i].is_idle) {
                int task_id = find_highest_priority_ready_task(dag);
                if (task_id != -1) {
                    Task* task = &dag->tasks[task_id];
                    cores[i].current_task = task;
                    cores[i].is_idle = false;
                    cores[i].time_slice_remaining = quantum;
                    
                    task->core_assigned = i;
                    if (task->start_time == -1) {
                        task->start_time = simulation_time;
                    }
                    
                    print_execution_trace(simulation_time, i, task, "Started");
                    printf("Executing Task %d (%s) on Core %d (Period: %d ms, Priority: %d)\n", 
                           task->id, task->name, i, task->period, task->priority);
                }
            }
        }
        
        // Update simulation time
        simulation_time++;

        for (int i = 0; i < num_cores; i++) {
            if (cores[i].is_idle) {
                cores[i].total_idle_time++;  // Increment idle time counter
            }
        }
        
        // Show progress
        if (simulation_time % 20 == 0) {
            print_progress_bar(completed_tasks, dag->num_tasks);
        }
        
        // Add small delay for visualization
        delay_ms(10); 
        
        // Safety check - prevent infinite loops
        if (simulation_time > 10000) {
            printf("\nSimulation exceeded time limit. Possible deadlock or very long tasks.\n");
            break;
        }
    }
    
    printf("\nSimulation completed in %d time units\n", simulation_time);
    
    // Print results
    printf("\n===== Execution Results with Rate Monotonic Scheduling =====\n");
    printf("ID | Name       | Duration | Period  | Priority | Start | Finish | Turnaround\n");
    printf("-------------------------------------------------------------------\n");
    
    int total_turnaround = 0;
    for (int i = 0; i < dag->num_tasks; i++) {
        Task* task = &dag->tasks[i];
        int turnaround = task->finish_time - task->start_time;
        total_turnaround += turnaround;
        
        printf("%-2d | %-10s | %-8d | %-7d | %-8d | %-5d | %-6d | %-10d\n",
               task->id, task->name, task->duration, task->period, task->priority,
               task->start_time, task->finish_time, turnaround);
    }
    
    printf("\nAverage Turnaround Time: %.2f\n", (float)total_turnaround / dag->num_tasks);

    printf("\n===== Core Utilization Statistics =====\n");
    printf("Core | Busy Time | Idle Time | Utilization %%\n");
    printf("----------------------------------------\n");

    float total_utilization = 0.0;
    for (int i = 0; i < num_cores; i++) {
        int busy_time = simulation_time - cores[i].total_idle_time;
        float utilization = (float)busy_time / simulation_time * 100.0;
        total_utilization += utilization;
        
        printf("%-4d | %-9d | %-9d | %.2f%%\n",
               i, busy_time, cores[i].total_idle_time, utilization);
    }

    printf("\nAverage Core Utilization: %.2f%%\n", total_utilization / num_cores);
    
    // Clean up
    free(cores);
    cores = NULL;
}

void run_performance_comparison(int num_cores) {
    if (!current_dag) {
        printf("No DAG available. Creating sample DAG...\n");
        current_dag = create_sample_dag();
    }
    
    printf("\n----- Performance Comparison with Rate Monotonic Scheduling -----\n");
    printf("DAG: %s\n", "Sample DAG");
    printf("Number of Tasks: %d\n", current_dag->num_tasks);
    printf("Number of Cores: %d\n", num_cores);
    
    // Run hybrid scheduler with RMS
    simulate_hybrid_scheduler(current_dag, num_cores);
    
    printf("\nPerformance comparison completed.\n");
}

void export_results_to_csv(DAG* dag, char* scheduler_name, int num_cores) {
    if (!dag) {
        printf("No DAG results available to export.\n");
        return;
    }
    
    char filename[100];
    sprintf(filename, "scheduler_results_%s_%d_cores.csv", scheduler_name, num_cores);
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Failed to create CSV file.\n");
        return;
    }
    
    // Write header
    fprintf(file, "Task ID,Task Name,Duration,Period,Priority,Start Time,Finish Time,Turnaround Time\n");
    
    // Write data
    for (int i = 0; i < dag->num_tasks; i++) {
        Task* task = &dag->tasks[i];
        int turnaround = task->finish_time - task->start_time;
        
        fprintf(file, "%d,%s,%d,%d,%d,%d,%d,%d\n",
                task->id, task->name, task->duration, task->period, task->priority,
                task->start_time, task->finish_time, turnaround);
    }
    
    fclose(file);
    printf("Results exported to %s\n", filename);
    char util_filename[100];
    sprintf(util_filename, "core_utilization_%s_%d_cores.csv", scheduler_name, num_cores);
    
    FILE* util_file = fopen(util_filename, "w");
    if (!util_file) {
        printf("Failed to create core utilization CSV file.\n");
        return;
    }
    
    // Write header
    fprintf(util_file, "Core ID,Busy Time,Idle Time,Utilization\n");
    
    // Write data
    for (int i = 0; i < num_cores; i++) {
        int busy_time = simulation_time - cores[i].total_idle_time;
        float utilization = (float)busy_time / simulation_time * 100.0;
        
        fprintf(util_file, "%d,%d,%d,%.2f\n",
                i, busy_time, cores[i].total_idle_time, utilization);
    }
    
    fclose(util_file);
    printf("Core utilization metrics exported to %s\n", util_filename);
}

void free_dag(DAG* dag) {
    if (!dag) return;
    
    // Free adjacency matrix
    if (dag->adjacency_matrix) {
        for (int i = 0; i < dag->num_tasks; i++) {
            free(dag->adjacency_matrix[i]);
        }
        free(dag->adjacency_matrix);
    }
    
    // Free tasks
    if (dag->tasks) {
        free(dag->tasks);
    }
    
    free(dag);
}

int main() {
    int choice;
    int num_cores = 4;
    bool exit_program = false;
    
    // Seed random number generator
    srand(time(NULL));
    
    while (!exit_program) {
        printf("\nHybrid DAG-Based Multi-Core Scheduler with RMS - Main Menu\n");
        printf("=============================================\n");
        printf("1. Create Sample DAG\n");
        printf("2. Create Custom DAG\n");
        printf("3. Display Current DAG\n");
        printf("4. Run Performance Comparison\n");
        printf("5. Export Results to CSV\n");
        printf("6. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        
        switch (choice) {
            case 1:
                if (current_dag) {
                    free_dag(current_dag);
                }
                current_dag = create_sample_dag();
                break;
                
            case 2:
                if (current_dag) {
                    free_dag(current_dag);
                }
                current_dag = create_custom_dag();
                break;
                
            case 3:
                display_dag(current_dag);
                break;
                
            case 4:
                printf("Enter number of cores (1-%d): ", MAX_CORES);
                scanf("%d", &num_cores);
                
                if (num_cores < 1 || num_cores > MAX_CORES) {
                    printf("Invalid number of cores. Using 4 cores.\n");
                    num_cores = 4;
                }
                
                printf("Enter quantum for Round Robin (in ms): ");
                scanf("%d", &quantum);
                
                if (quantum < MIN_QUANTUM) {
                    printf("Invalid quantum. Using default (%d ms).\n", DEFAULT_QUANTUM);
                    quantum = DEFAULT_QUANTUM;
                }
                
                // Enable debug mode for detailed execution trace
                printf("Enable debug mode? (0-No, 1-Yes): ");
                scanf("%d", (int*)&debug_mode);
                
                run_performance_comparison(num_cores);
                break;
                
            case 5:
                if (!current_dag) {
                    printf("No results available to export. Run a simulation first.\n");
                } else {
                    export_results_to_csv(current_dag, "hybrid_rms", num_cores);
                }
                break;
                
            case 6:
                exit_program = true;
                break;
                
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }
    
    // Clean up
    if (current_dag) {
        free_dag(current_dag);
    }
    
    printf("Program terminated.\n");
    return 0;
}
