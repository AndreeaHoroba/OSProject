#include<stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif

#define MAX_PATH_LEN 256

struct EntryMetadata {
    char path[MAX_PATH_LEN];
    time_t timestamp;
    off_t size;
    mode_t permissions;
    ino_t inode;
    time_t last_modified;
};

void gather_metadata(struct EntryMetadata *metadata, const char *path)
{
    struct stat file_stat;
    if (stat(path, &file_stat) == 0) {
        strcpy(metadata->path, path);
        metadata->timestamp = time(NULL);
        metadata->size = file_stat.st_size;
        metadata->permissions = file_stat.st_mode;
        metadata->inode = file_stat.st_ino;
        metadata->last_modified = file_stat.st_mtime;
    }
}

void write_metadata(int fd, const struct EntryMetadata *metadata)
{
    dprintf(fd, "File: %s\n", metadata->path);
    dprintf(fd, "Timestamp: %ld\n", metadata->timestamp);
    dprintf(fd, "Entry: %s\n", metadata->path);
    dprintf(fd, "Size: %ld\n", metadata->size);
    dprintf(fd, "Last modified: %ld\n", metadata->last_modified);
    dprintf(fd, "Permissions: %o\n", metadata->permissions);
    dprintf(fd, "Inode no: %ld\n\n", metadata->inode);
}

void traverse_directory(DIR *dir, int fd, const char *directory)
{
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char entryPath[MAX_PATH_LEN];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", directory, entry->d_name);
        struct EntryMetadata metadata;
        gather_metadata(&metadata, entryPath);
        write_metadata(fd, &metadata);
        if (entry->d_type == DT_DIR) {
            DIR *subDir = opendir(entryPath);
            if (subDir != NULL) {
                traverse_directory(subDir, fd, entryPath);
                closedir(subDir);
            }
        }
    }
}

int main(int argc, char *argv[])
 {
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("Pipe creation failed");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return 1;
        } else if (pid == 0) { // Child process
            close(pfd[0]);

            // Create a snapshot, write to pipe
            DIR *dir = opendir(argv[i]);
            if (dir == NULL) {
                perror("opendir");
                close(pfd[1]);
                exit(1);
            }
            traverse_directory(dir, pfd[1], argv[i]);
            closedir(dir);
            close(pfd[1]);
            exit(0);
        }
    }

    close(pfd[1]);

    // Parent reads from pipe
    char buffer[1024];
    FILE *snapshotFile = fopen("snapshot_results.txt", "w");
    if (!snapshotFile) {
        perror("Failed to open file for writing");
        close(pfd[0]);
        return 1;
    }
    int nbytes;
    while ((nbytes = read(pfd[0], buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, nbytes, snapshotFile);
    }
    fclose(snapshotFile);
    close(pfd[0]);

    // Wait for all child processes to exit
    for (int i = 1; i < argc; i++) {
        wait(NULL);
    }

    return 0;
}
