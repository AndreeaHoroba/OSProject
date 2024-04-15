#include<stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define MAX_FILES 1000 // Maximum number of files in the directory
#define PATH_MAX 1000

// Struct to store file information
typedef struct {
    char *name;
    time_t last_modified;
    int size;
} FileInfo;

// Function to compare two file information structs by name
int compare_file_info(const void *a, const void *b) {
    return strcmp(((const FileInfo *)a)->name, ((const FileInfo *)b)->name);
}

void process_directory(const char *directory_path, int output_fd) {
    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        exit(1);
    }

    // Read the directory and store file information in an array
    struct dirent *entry;
    FileInfo files[MAX_FILES];
    int num_files = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char file_path[PATH_MAX];
            sprintf(file_path, "%s/%s", directory_path, entry->d_name);
            struct stat file_stat;
            if (stat(file_path, &file_stat) == 0) {
                files[num_files].name = strdup(entry->d_name);
                files[num_files].last_modified = file_stat.st_mtime;
                files[num_files].size = file_stat.st_size;
                num_files++;
            }
        }
    }
    closedir(dir);

    // Sort the file information array by name
    qsort(files, num_files, sizeof(FileInfo), compare_file_info);

    // Print the initial snapshot
    char initial_snapshot_msg[1000];
    sprintf(initial_snapshot_msg, "Initial Snapshot of %s:\n", directory_path);
    write(output_fd, initial_snapshot_msg, strlen(initial_snapshot_msg));
    for (int i = 0; i < num_files; i++) {
        char file_info[PATH_MAX + 100];
        sprintf(file_info, "%s\n", files[i].name);
        write(output_fd, file_info, strlen(file_info));
    }

    // Open the directory again to take the second snapshot
    dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        exit(1);
    }

    // Read the directory again and compare with the initial snapshot
    char changes_msg[1000];
    sprintf(changes_msg, "\nChanges in %s:\n", directory_path);
    write(output_fd, changes_msg, strlen(changes_msg));
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char file_path[PATH_MAX];
            sprintf(file_path, "%s/%s", directory_path, entry->d_name);
            struct stat file_stat;
            if (stat(file_path, &file_stat) == 0) {
                // Search for the file in the initial snapshot
                FileInfo key = {.name = entry->d_name};
                FileInfo *found_file = bsearch(&key, files, num_files, sizeof(FileInfo), compare_file_info);
                if (found_file == NULL) {
                    char added_msg[PATH_MAX + 100];
                    sprintf(added_msg, "Added: %s\n", entry->d_name);
                    write(output_fd, added_msg, strlen(added_msg));
                } else {
                    // Compare last modified time and size
                    if (found_file->last_modified != file_stat.st_mtime || found_file->size != file_stat.st_size) {
                        char modified_msg[PATH_MAX + 100];
                        sprintf(modified_msg, "Modified: %s\n", entry->d_name);
                        write(output_fd, modified_msg, strlen(modified_msg));
                    }
                }
            }
        }
    }

    // Deallocate memory for file names
    for (int i = 0; i < num_files; i++) {
        free(files[i].name);
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    // Check if the directory path is provided as a command-line argument
    if (argc != 2) {
        printf("Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe failed");
        return 1;
    }

    // Create a child process
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        close(pipe_fds[0]); // Close the read end of the pipe in the child process
        process_directory(argv[1], pipe_fds[1]); // Pass the write end of the pipe
        close(pipe_fds[1]); // Close the write end of the pipe after finishing writing
        exit(0);
    } else {
        // Parent process
        close(pipe_fds[1]); // Close the write end of the pipe in the parent process
        char buffer[1000];
        ssize_t nbytes;
        while ((nbytes = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, nbytes); // Write to standard output
        }
        close(pipe_fds[0]); // Close the read end of the pipe after finishing reading
        printf("\nParent process finished.\n");
    }

    return 0;
}
