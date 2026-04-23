#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>

#define MAX_STR 64
#define DESC_STR 256

typedef enum { ROLE_INSPECTOR, ROLE_MANAGER, ROLE_UNKNOWN } Role;

typedef struct {
    int id;
    char inspector[MAX_STR];
    float lat;
    float lon;
    char category[MAX_STR];
    int severity;
    time_t timestamp;
    char description[DESC_STR];
} Report;

// Global context
Role current_role = ROLE_UNKNOWN;
char current_user[MAX_STR] = "unknown";
char district[MAX_STR] = "";

// Helper: Convert permission bits to string (e.g., rw-rw-r--)
void perms_to_string(mode_t mode, char *str) {
    strcpy(str, "---------");
    if (mode & S_IRUSR) str[0] = 'r';
    if (mode & S_IWUSR) str[1] = 'w';
    if (mode & S_IXUSR) str[2] = 'x';
    if (mode & S_IRGRP) str[3] = 'r';
    if (mode & S_IWGRP) str[4] = 'w';
    if (mode & S_IXGRP) str[5] = 'x';
    if (mode & S_IROTH) str[6] = 'r';
    if (mode & S_IWOTH) str[7] = 'w';
    if (mode & S_IXOTH) str[8] = 'x';
}

// Helper: Check if current role has a specific permission
int has_permission(mode_t mode, int read_req, int write_req) {
    if (current_role == ROLE_MANAGER) {
        if (read_req && !(mode & S_IRUSR)) return 0;
        if (write_req && !(mode & S_IWUSR)) return 0;
    } else if (current_role == ROLE_INSPECTOR) {
        if (read_req && !(mode & S_IRGRP)) return 0;
        if (write_req && !(mode & S_IWGRP)) return 0;
    }
    return 1;
}

// Helper: Log operation to logged_district
void log_operation(const char *op) {
    char path[512];
    snprintf(path, sizeof(path), "%s/logged_district", district);
    
    struct stat st;
    if (stat(path, &st) == 0) {
        if (!has_permission(st.st_mode, 0, 1)) {
            // Silently fail logging if no permission (e.g., inspector trying to log)
            return; 
        }
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        chmod(path, 0644); // Ensure correct permissions
        char log_entry[512];
        snprintf(log_entry, sizeof(log_entry), "%ld\n%s\n%s %s\n", 
                 (long)time(NULL), current_user, 
                 current_role == ROLE_MANAGER ? "manager" : "inspector", op);
        write(fd, log_entry, strlen(log_entry));
        close(fd);
    }
}

// Ensure directory and core files exist, manage symlink
void setup_district() {
    struct stat st;
    if (stat(district, &st) == -1) {
        mkdir(district, 0750);
    }

    char path[512];
    
    // Config file
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    if (stat(path, &st) == -1) {
        int fd = open(path, O_WRONLY | O_CREAT, 0640);
        write(fd, "3\n", 2); // Default threshold
        close(fd);
    }

    // Reports file
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    if (stat(path, &st) == -1) {
        int fd = open(path, O_WRONLY | O_CREAT, 0664);
        close(fd);
    }

    // Symlink logic
    char sym_name[512];
    snprintf(sym_name, sizeof(sym_name), "active_reports-%s", district);
    struct stat lst;
    if (lstat(sym_name, &lst) == -1) {
        symlink(path, sym_name);
    }
}

// Command: add
void do_add() {
    setup_district();
    log_operation("add");

    char path[512];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    
    struct stat st;
    if (stat(path, &st) == 0 && !has_permission(st.st_mode, 0, 1)) {
        printf("Error: Permission denied to write reports.dat\n");
        exit(1);
    }

    Report r;
    memset(&r, 0, sizeof(Report));
    
    // Find the highest existing ID to prevent duplicates after a removal
    int max_id = -1;
    int fd_check = open(path, O_RDONLY);
    if (fd_check != -1) {
        Report temp_r;
        while (read(fd_check, &temp_r, sizeof(Report)) == sizeof(Report)) {
            if (temp_r.id > max_id) {
                max_id = temp_r.id;
            }
        }
        close(fd_check);
    }
    
    r.id = max_id + 1;
    strncpy(r.inspector, current_user, MAX_STR);
    r.timestamp = time(NULL);

    printf("X: "); scanf("%f", &r.lat);
    printf("Y: "); scanf("%f", &r.lon);
    printf("Category (road/lighting/flooding/other): "); scanf("%s", r.category);
    printf("Severity level (1/2/3): "); scanf("%d", &r.severity);
    
    // Flush stdin before reading description
    int c; while ((c = getchar()) != '\n' && c != EOF);
    
    printf("Description: ");
    fgets(r.description, DESC_STR, stdin);
    r.description[strcspn(r.description, "\n")] = 0; // Remove newline

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd != -1) {
        write(fd, &r, sizeof(Report));
        close(fd);
        chmod(path, 0664); // Ensure 664 per spec
    }
}

// Command: list
void do_list() {
    log_operation("list");
    char path[512];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    struct stat st;
    if (stat(path, &st) == -1 || !has_permission(st.st_mode, 1, 0)) {
        printf("Error: Permission denied to read reports.dat\n");
        return;
    }

    char perms[10];
    perms_to_string(st.st_mode, perms);
    printf("File: %s | Size: %ld bytes | Perms: %s | Modified: %ld\n", 
           path, (long)st.st_size, perms, (long)st.st_mtime);

    int fd = open(path, O_RDONLY);
    if (fd == -1) return;

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        printf("ID: %d | Cat: %s | Sev: %d | By: %s\n", r.id, r.category, r.severity, r.inspector);
    }
    close(fd);
}

// Command: remove_report
void do_remove(int target_id) {
    if (current_role != ROLE_MANAGER) {
        printf("Error: Only managers can remove reports.\n");
        return;
    }
    log_operation("remove");

    char path[512];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    int fd = open(path, O_RDWR);
    if (fd == -1) return;

    struct stat st;
    fstat(fd, &st);
    int total_records = st.st_size / sizeof(Report);
    
    int found_index = -1;
    Report r;
    
    for (int i = 0; i < total_records; i++) {
        read(fd, &r, sizeof(Report));
        if (r.id == target_id) {
            found_index = i;
            break;
        }
    }

    if (found_index != -1) {
        for (int i = found_index + 1; i < total_records; i++) {
            lseek(fd, i * sizeof(Report), SEEK_SET);
            read(fd, &r, sizeof(Report));
            
            lseek(fd, (i - 1) * sizeof(Report), SEEK_SET);
            write(fd, &r, sizeof(Report));
        }
        ftruncate(fd, (total_records - 1) * sizeof(Report));
        printf("Report %d removed.\n", target_id);
    } else {
        printf("Report %d not found.\n", target_id);
    }
    close(fd);
}

// AI-Assisted Filter Functions (Stubs for parse_condition & match_condition)
int parse_condition(const char *input, char *field, char *op, char *value) {
    // Basic implementation splitting "field:op:value"
    return sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == val;
        if (strcmp(op, ">=") == 0) return r->severity >= val;
        // Add <, <=, >, != as needed
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
    }
    return 0; // Default false
}


// Command: view
void do_view(int target_id) {
    log_operation("view");
    char path[512];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    struct stat st;
    if (stat(path, &st) == -1 || !has_permission(st.st_mode, 1, 0)) {
        printf("Error: Permission denied to read reports.dat\n");
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("Error: Could not open reports.dat\n");
        return;
    }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == target_id) {
            // Convert the raw timestamp into a readable date string
            struct tm *tm_info = localtime(&r.timestamp);
            char time_buf[26];
            strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", tm_info);

            printf("\n--- Report %d Details ---\n", r.id);
            printf("Inspector:   %s\n", r.inspector);
            printf("Timestamp:   %s\n", time_buf);
            printf("Location:    X: %.4f | Y: %.4f\n", r.lat, r.lon);
            printf("Category:    %s\n", r.category);
            printf("Severity:    %d\n", r.severity);
            printf("Description: %s\n", r.description);
            printf("------------------------\n\n");
            
            found = 1;
            break; // Stop searching once we find it
        }
    }

    if (!found) {
        printf("Report %d not found in district '%s'.\n", target_id, district);
    }

    close(fd);
}


// Command: filter
void do_filter(int argc, char *argv[], int start_idx) {
    log_operation("filter");
    char path[512];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("Error: Could not open reports.dat or district does not exist.\n");
        return;
    }

    Report r;
    // Read through every report in the binary file
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int matches_all = 1; 

        // Loop through all provided conditions on the command line
        for (int i = start_idx; i < argc; i++) {
            char field[MAX_STR], op[MAX_STR], value[MAX_STR];
            
            if (!parse_condition(argv[i], field, op, value)) {
                printf("Invalid filter condition format: %s\n", argv[i]);
                matches_all = 0;
                break;
            }
            
            // If it fails even one condition, it's not a match (AND logic)
            if (!match_condition(&r, field, op, value)) {
                matches_all = 0;
                break;
            }
        }

        if (matches_all) {
            printf("Match Found - ID: %d | Cat: %s | Sev: %d | By: %s\n", 
                   r.id, r.category, r.severity, r.inspector);
        }
    }
    close(fd);
}

// Command: update_threshold
void do_update_threshold(int new_threshold) {
    if (current_role != ROLE_MANAGER) {
        printf("Error: Only managers can update the threshold.\n");
        return;
    }
    log_operation("update_threshold");

    char path[512];
    snprintf(path, sizeof(path), "%s/district.cfg", district);

    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Error: district.cfg not found.\n");
        return;
    }

    // Verify permission bits match exactly 0640 (rw-r-----)
    // We mask the st_mode with 0777 (S_IRWXU | S_IRWXG | S_IRWXO) to isolate just the file permissions
    if ((st.st_mode & 0777) != 0640) {
        printf("Diagnostic Error: Permissions for district.cfg have been altered from 0640. Refusing to update.\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd != -1) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", new_threshold);
        write(fd, buf, strlen(buf));
        close(fd);
        printf("Severity threshold updated to %d.\n", new_threshold);
    } else {
        printf("Error: Could not open district.cfg for writing.\n");
    }
}

// Helper: Check for broken symlinks in the current directory
void check_dangling_symlinks() {
    DIR *dir = opendir(".");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Look for files starting with "active_reports-"
        if (strncmp(entry->d_name, "active_reports-", 15) == 0) {
            struct stat lst;
            // Use lstat() to check the link itself
            if (lstat(entry->d_name, &lst) == 0 && S_ISLNK(lst.st_mode)) {
                struct stat target_st;
                // Use stat() to check if the target it points to actually exists
                if (stat(entry->d_name, &target_st) == -1) {
                    printf("Warning: Dangling symlink detected: %s points to a missing file.\n", entry->d_name);
                }
            }
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s --role <role> --user <user> --<command> <district_id> [args...]\n", argv[0]);
        return 1;
    }

    int cmd_index = 0;
    
    // Parse Arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "manager") == 0) current_role = ROLE_MANAGER;
            else if (strcmp(argv[i+1], "inspector") == 0) current_role = ROLE_INSPECTOR;
            i++;
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strncpy(current_user, argv[i+1], MAX_STR);
            i++;
        } else if (strncmp(argv[i], "--", 2) == 0) {
            cmd_index = i;
            if (i + 1 < argc) strncpy(district, argv[i+1], MAX_STR);
            break;
        }
    }

    check_dangling_symlinks();

    if (current_role == ROLE_UNKNOWN || strlen(district) == 0 || cmd_index == 0) {
        printf("Missing required parameters.\n");
        return 1;
    }

    // Dispatch Command
    char *cmd = argv[cmd_index];
    
    if (strcmp(cmd, "--add") == 0) {
        do_add();
    } else if (strcmp(cmd, "--list") == 0) {
        do_list();
    } 
    else if (strcmp(cmd, "--view") == 0) { // <--- ADD THIS BLOCK
        if (cmd_index + 2 < argc) {
            do_view(atoi(argv[cmd_index + 2]));
        } else {
            printf("Error: Missing report ID to view.\n");
        }
    }
    else if (strcmp(cmd, "--remove_report") == 0) {
        if (cmd_index + 2 < argc) {
            do_remove(atoi(argv[cmd_index + 2]));
        }
    } else if (strcmp(cmd, "--filter") == 0) {
        if (cmd_index + 2 < argc) {
            do_filter(argc , argv, cmd_index + 2);
        }
        else{
            printf("Error: Missing filter condition\n");
        }
    }
    else if (strcmp(cmd, "--update_threshold") == 0) { // <--- ADD THIS BLOCK
        if (cmd_index + 2 < argc) {
            do_update_threshold(atoi(argv[cmd_index + 2]));
        }
    }
    else {
        printf("Unknown command: %s\n", cmd);
    }

    return 0;
}