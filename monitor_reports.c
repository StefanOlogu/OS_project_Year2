#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

// Global flags for signal handlers
volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigusr1_received = 0;

// Handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    sigint_received = 1;
}

// Handler for SIGUSR1 (New report added)
void handle_sigusr1(int sig) {
    sigusr1_received = 1;
}

int main() {
    // 1. Set up sigaction for SIGINT
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    // 2. Set up sigaction for SIGUSR1
    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    // 3. Create or overwrite the hidden .monitor_pid file
    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error: Could not create .monitor_pid");
        return 1;
    }
    
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    write(fd, pid_str, strlen(pid_str));
    close(fd);

    printf("Monitor started with PID: %d. Waiting for events...\n", getpid());

    // 4. Main loop: Wait until SIGINT is received
    while (!sigint_received) {
        pause(); // Suspend execution until ANY signal arrives
        
        if (sigusr1_received) {
            printf("Monitor: A new report has been added to the system!\n");
            sigusr1_received = 0; // Reset flag
        }
    }

    // 5. Cleanup on exit
    printf("\nMonitor: SIGINT received. Deleting .monitor_pid and shutting down...\n");
    unlink(".monitor_pid");
    
    return 0;
}