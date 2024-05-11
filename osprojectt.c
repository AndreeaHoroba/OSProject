#include<stdio.h>
#include<stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SIZE 1024

void checkForMalware(const char *filePath, char *quarantinePath) {
    char *filename = strrchr(filePath, '/') + 1;
    if (!quarantinePath) return;
    char *newPath = malloc(strlen(quarantinePath) + strlen(filename) + 2);
    sprintf(newPath, "%s/%s", quarantinePath, filename);
    rename(filePath, newPath);
    free(newPath);
}

char* getFileInfo(const char *filePath, char *quarantinePath) {
    struct stat fileStat;
    if (stat(filePath, &fileStat) == -1) {
        perror("Error retrieving file information");
        exit(EXIT_FAILURE);
    }
    char *infoString = malloc(SIZE * sizeof(char));
    if (!infoString) {
        perror("Error allocating memory for file information");
        exit(EXIT_FAILURE);
    }
    sprintf(infoString, "\"%s\"/%ld/%s%s%s%s%s%s%s%s%s/%ld/%ld\n",
        filePath,
        fileStat.st_size,
        (fileStat.st_mode & S_IRUSR) ? "r" : "-",
        (fileStat.st_mode & S_IWUSR) ? "w" : "-",
        (fileStat.st_mode & S_IXUSR) ? "x" : "-",
        (fileStat.st_mode & S_IRGRP) ? "r" : "-",
        (fileStat.st_mode & S_IWGRP) ? "w" : "-",
        (fileStat.st_mode & S_IXGRP) ? "x" : "-",
        (fileStat.st_mode & S_IROTH) ? "r" : "-",
        (fileStat.st_mode & S_IWOTH) ? "w" : "-",
        (fileStat.st_mode & S_IXOTH) ? "x" : "-",
        fileStat.st_ino,
        fileStat.st_mtime
    );
    if (strstr(infoString, "---------") != NULL) {
        checkForMalware(filePath, quarantinePath);
        strcpy(infoString, "");
    }
    return infoString;
}

int isDirectory(const char *path) {
    struct stat statbuf;
    return stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
}

void freeFileInfo(char *infoString) {
    free(infoString);
}

int traverseDirectory(const char *dirPath, char *text, char *quarantinePath) {
    DIR *dir = opendir(dirPath);
    if (!dir) {
        perror("Error opening directory");
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char filePath[1024];
        snprintf(filePath, sizeof(filePath), "%s/%s", dirPath, entry->d_name);
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (!isDirectory(filePath)) {
            char *fileInfo = getFileInfo(filePath, quarantinePath);
            if (fileInfo != NULL) {
                strcat(text, fileInfo);
                freeFileInfo(fileInfo);
            }
        } else {
            traverseDirectory(filePath, text, quarantinePath);
        }
    }
    closedir(dir);
    return 1;
}

char* readFile(const char *filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return NULL;
    }
    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1) {
        perror("Error getting file size");
        close(fd);
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);
    char *content = malloc((fileSize + 1) * sizeof(char));
    if (!content) {
        perror("Error allocating memory");
        close(fd);
        return NULL;
    }
    ssize_t bytesRead = read(fd, content, fileSize);
    if (bytesRead == -1) {
        perror("Error reading from file");
        close(fd);
        free(content);
        return NULL;
    }
    content[bytesRead] = '\0';
    close(fd);
    return content;
}

void createSnapshot(const char *filePath, char *text) {
    if (access(filePath, F_OK) != -1) {
        char *fileContent = readFile(filePath);
        if (fileContent != NULL) {
            if (strstr(fileContent, text) == NULL || strlen(fileContent) != strlen(text)) {
                int fd = open(filePath, O_WRONLY | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("Error opening file for writing");
                    return;
                }
                if (write(fd, text, strlen(text)) == -1) {
                    perror("Error writing to file");
                    close(fd);
                    return;
                }
                printf("Snapshot Updated for %s\n", filePath);
            } else {
                printf("No changes were made to %s.\n", filePath);
            }
            free(fileContent);
        }
    } else {
        int fd = open(filePath, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) {
            perror("Error creating file");
            return;
        }
        printf("Snapshot created successfully: %s\n", filePath);
        if (write(fd, text, strlen(text)) == -1) {
            perror("Error writing to file");
            close(fd);
            return;
        }
        close(fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 14) {
        fprintf(stderr, "Usage: %s <file_path> ... (-o -> output, -s -> quarantine)\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *outputDir = NULL;
    char *quarantineDir = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp("-o", argv[i])) {
            outputDir = argv[++i];
            if (outputDir[strlen(outputDir) - 1] == '/')
                outputDir[strlen(outputDir) - 1] = '\0';
        } else if (!strcmp("-s", argv[i])) {
            quarantineDir = argv[++i];
            if (quarantineDir[strlen(quarantineDir) - 1] == '/')
                quarantineDir[strlen(quarantineDir) - 1] = '\0';
        }
    }
    char text[SIZE];
    strcpy(text, "Path/Size/Permissions/INode/TimeOfLastMod\n");
    for (int i = 1; i < argc; i++) {
        if (!strcmp("-o", argv[i]) || !strcmp("-s", argv[i])) {
            i++;
            continue;
        }
        if (argv[i][strlen(argv[i]) - 1] == '/')
            argv[i][strlen(argv[i]) - 1] = '\0';
        int fd[2];
        if (pipe(fd) == -1) {
            perror("Pipe failed");
            return EXIT_FAILURE;
        }
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            close(fd[1]);
            char dirPath[SIZE];
            read(fd[0], dirPath, SIZE);
            traverseDirectory(dirPath, text, quarantineDir);
            char snap[SIZE];
            if (outputDir != NULL)
                snprintf(snap, sizeof(snap), "%s/%s_snapshot.txt", outputDir, dirPath);
            else
                snprintf(snap, sizeof(snap), "%s_snapshot.txt", dirPath);
            createSnapshot(snap, text);
            close(fd[0]);
            exit(i);
        } else {
            close(fd[0]); 
            write(fd[1], argv[i], strlen(argv[i]));
            close(fd[1]);  
        }
    }
    int pidValue[argc];
    int exitValue[argc];
    for (int i = 1; i < argc; i++) {
        if (!strcmp("-o", argv[i]) || !strcmp("-s", argv[i])) {
            i++;
            continue;
        }
        pidValue[i - 1] = wait(&exitValue[i - 1]);
    }
    printf("\n");
    for (int i = 1; i < argc; i++) {
        if (!strcmp("-o", argv[i]) || !strcmp("-s", argv[i])) {
            i++;
            continue;
        }
        printf("Process with pid %d ended with code %d\n", pidValue[i - 1], exitValue[i - 1]);
    }
    return EXIT_SUCCESS;
}
