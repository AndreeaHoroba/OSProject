#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_FILES 1000 // Maximum number of files in the directory

// Struct to store file information
typedef struct {
    char *name;
    time_t last_modified;
    off_t size;
} FileInfo;

// Function to compare two file information structs by name
int compare_file_info(const void *a, const void *b) {
    return strcmp(((const FileInfo *)a)->name, ((const FileInfo *)b)->name);
}

int main(int argc, char *argv[]) {
    // Check if the directory path is provided as a command-line argument
    if (argc != 2) {
        printf("Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    // Open the directory
    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("Unable to open directory");
        return 1;
    }

    // Read the directory and store file information in an array
    struct dirent *entry;
    FileInfo files[MAX_FILES];
    int num_files = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char file_path[PATH_MAX];
            sprintf(file_path, "%s/%s", argv[1], entry->d_name);
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
    printf("Initial Snapshot:\n");
    for (int i = 0; i < num_files; i++) {
        printf("%s\n", files[i].name);
    }

    // Open the directory again to take the second snapshot
    dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("Unable to open directory");
        return 1;
    }

    // Read the directory again and compare with the initial snapshot
    printf("\nChanges:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char file_path[PATH_MAX];
            sprintf(file_path, "%s/%s", argv[1], entry->d_name);
            struct stat file_stat;
            if (stat(file_path, &file_stat) == 0) {
                // Search for the file in the initial snapshot
                FileInfo key = {.name = entry->d_name};
                FileInfo *found_file = bsearch(&key, files, num_files, sizeof(FileInfo), compare_file_info);
                if (found_file == NULL) {
                    printf("Added: %s\n", entry->d_name);
                } else {
                    // Compare last modified time and size
                    if (found_file->last_modified != file_stat.st_mtime || found_file->size != file_stat.st_size) {
                        printf("Modified: %s\n", entry->d_name);
                    }
                }
            }
        }
    }

    // Close the directory
    closedir(dir);

    // Deallocate memory for file names
    for (int i = 0; i < num_files; i++) {
        free(files[i].name);
    }

    return 0;
}
