#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

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

void gather_metadata(struct EntryMetadata *metadata, const char *path) {
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

void write_metadata(FILE *snapshotFile, const struct EntryMetadata *metadata) {
    fprintf(snapshotFile, "File: %s\n", metadata->path);
    fprintf(snapshotFile, "Timestamp: %ld\n", metadata->timestamp);
    fprintf(snapshotFile, "Entry: %s\n", metadata->path);
    fprintf(snapshotFile, "Size: %ld\n", metadata->size);
    fprintf(snapshotFile, "Last modified: %ld\n", metadata->last_modified);
    fprintf(snapshotFile, "Permissions: %o\n", metadata->permissions);
    fprintf(snapshotFile, "Inode no: %ld\n\n", metadata->inode);
}

int has_directory_changed(DIR *dir, FILE *snapshotFile, const char *directory) {
}

void traverse_directory(DIR *dir, FILE *snapshotFile, const char *directory) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char entryPath[MAX_PATH_LEN];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", directory, entry->d_name);
        struct EntryMetadata metadata;
        gather_metadata(&metadata, entryPath);
        write_metadata(snapshotFile, &metadata);
        if (entry->d_type == DT_DIR) {
            DIR *subDir = opendir(entryPath);
            if (subDir != NULL) {
                traverse_directory(subDir, snapshotFile, entryPath);
                closedir(subDir);
            }
        }
    }
}

void create_snapshot(const char *directory) {
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        perror("opendir");
        return;
    }
    
    struct stat st = {0};
    if (stat("allsnapshots", &st) == -1) {
        mkdir("allsnapshots", 0700);
    }

    char snapshotFilePath[MAX_PATH_LEN];
    snprintf(snapshotFilePath, sizeof(snapshotFilePath), "allsnapshots/snapshot_%s.txt", directory);

    FILE *snapshotFile = fopen(snapshotFilePath, "a+");
    if (snapshotFile == NULL) {
        perror("fopen");
        closedir(dir);
        return;
    }
    traverse_directory(dir, snapshotFile, directory);
    fclose(snapshotFile);
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        create_snapshot(argv[i]);
    }
    return 0;
}