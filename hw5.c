#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

typedef struct Task
{
    char srcFilePathTask[PATH_MAX];
    char destFilePathTask[PATH_MAX];
    int srcFd;
    int destFd;
    // void (*taskFunction)(int, int);
    // int arg1, arg2;
} Task;

Task taskQueue[1024];
int taskCount = 0;
char srcDirPath[PATH_MAX];
char destDirPath[PATH_MAX];
int bufferSize = 0;
int consumerSize = 0;
int doneFlag = 0;
long long totalBytes = 0;
int totalFolder = 0;
int totalFile = 0;
int totalFIFO = 0;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;
pthread_cond_t prodQueue;

void executeTask(Task *task);

void copyFile(const int srcFd, const int destFd);

void copyFilesInDirectory(const char *srcDirPath, const char *destDirPath);

void *startConsThread(void *args);

/* Produce value(s) */
void *startProdThread(void *args);

void handle_signal(int sg);

long long current_timestamp();

int main(int argc, char *argv[])
{
    struct timeval start_time, end_time;
    long long start_ms, end_ms, run_ms;
    
    gettimeofday(&start_time, NULL);
    start_ms = (start_time.tv_sec * 1000LL) + (start_time.tv_usec / 1000);


    if (argc != 5)
    {
        printf("Usage: %s BufferSize NumberOfConsumer SourceDir DestDir\n", argv[0]);
        return 1;
    }

    // Retrieve the command-line arguments
    bufferSize = atoi(argv[1]);   // Convert the first argument to an integer
    consumerSize = atoi(argv[2]); // Convert the first argument to an integer
    strcpy(srcDirPath, argv[3]);
    strcpy(destDirPath, argv[4]);

    // Register signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Use the retrieved values as needed
    pthread_t th_cons[consumerSize], th_prod;

    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    pthread_cond_init(&prodQueue, NULL);

    /* create the threads; may be any number, in general */
    if (pthread_create(&th_prod, NULL, &startProdThread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create the producer thread\n");
        exit(1);
    }

    int i;
    for (i = 0; i < consumerSize; i++)
    {
        if (pthread_create(&th_cons[i], NULL, &startConsThread, NULL) != 0)
        {
            perror("Failed to create the consumer thread");
        }
    }

    if (pthread_join(th_prod, NULL) != 0)
    {
        perror("Failed to join the producer thread");
    }

    for (i = 0; i < consumerSize; i++)
    {
        if (pthread_join(th_cons[i], NULL) != 0)
        {
            perror("Failed to join the consumer thread");
        }
    }

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    pthread_cond_destroy(&prodQueue);

    // Get the end time
    gettimeofday(&end_time, NULL);
    end_ms = (end_time.tv_sec * 1000LL) + (end_time.tv_usec / 1000);

    // Calculate the total run time
    run_ms = end_ms - start_ms;

    // Display the start time, end time, and total run time
    printf("\nProgram started at: %lld milliseconds\n", start_ms);
    printf("Program ended at: %lld milliseconds\n", end_ms);
    printf("Total run time: %lld milliseconds\n", run_ms);
    printf("Total copy btye: %lld btye\n", totalBytes);
    printf("Total copy Folder: %d \n", totalFolder);
    printf("Total copy File: %d \n", totalFile);
    printf("Total copy FIFO: %d \n", totalFIFO);

    return 0;
}

void *startConsThread(void *args)
{
    while (1)
    {
        Task task;
        //printf("Task Count = %d -- Done Flag = %d\n",taskCount,doneFlag);
        if (taskCount <= 0 && doneFlag == 1)
        {
            //printf("Buraya girdi! \n");
            //pthread_mutex_unlock(&mutexQueue);
            break;
        }
        else
        {
            pthread_mutex_lock(&mutexQueue);

            while (taskCount == 0 && doneFlag == 0)
            {
                pthread_cond_wait(&condQueue, &mutexQueue);
            }
            if(taskCount > 0 )
            {

            task = taskQueue[0];
            int i;
            for (i = 0; i < taskCount - 1; i++)
            {
                taskQueue[i] = taskQueue[i + 1];
            }
            taskCount--;
            printf("Task desc Path = %s\n", task.destFilePathTask);
            printf("Decreased Pool Count. Active Pool Count = %d\n", taskCount);
            pthread_mutex_unlock(&mutexQueue);
            executeTask(&task);
            pthread_cond_signal(&prodQueue);
            }
            else{
            pthread_mutex_unlock(&mutexQueue);

            }

        }
    }
    return NULL;
}

/* Produce value(s) */
void *startProdThread(void *args)
{
    copyFilesInDirectory(srcDirPath, destDirPath);

    pthread_mutex_lock(&mutexQueue);
    doneFlag = 1;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_broadcast(&condQueue);

    // pthread_cond_destroy(&condQueue);
    // pthread_exit(NULL);
    printf("\nProducer Finished Job and Closed ! \n\n");
    return NULL;
}

void copyFilesInDirectory(const char *srcDirPath, const char *destDirPath)
{
    DIR *srcDir;
    struct dirent *entry;
    struct stat fileStat;
    char srcFilePath[PATH_MAX];
    char destFilePath[PATH_MAX];
    int srcFileFd;
    int destFileFd;

    // Open source directory
    srcDir = opendir(srcDirPath);
    if (srcDir == NULL)
    {
        perror("Error opening source directory");
        return;
    }

    // Create destination directory if it doesn't exist
    if (mkdir(destDirPath, 0755) == -1 && errno != EEXIST)
    {
        perror("Error creating destination directory");
        closedir(srcDir);
        return;
    }

    // Iterate over directory entries
    while ((entry = readdir(srcDir)) != NULL)
    {
        // Exclude current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Build source and destination file paths
        snprintf(srcFilePath, PATH_MAX, "%s/%s", srcDirPath, entry->d_name);
        snprintf(destFilePath, PATH_MAX, "%s/%s", destDirPath, entry->d_name);

        // Get file information
        if (stat(srcFilePath, &fileStat) == -1)
        {
            perror("Error getting file information");
            continue;
        }

        if (S_ISDIR(fileStat.st_mode))
        {
            totalFolder++;
            // Recursively copy subdirectory
            copyFilesInDirectory(srcFilePath, destFilePath);
        }
        else if (S_ISREG(fileStat.st_mode))
        {
            totalFile++;
            // Copy regular file to the destination directory
            pthread_mutex_lock(&mutexQueue);

            // Open source file for reading
            if (stat(srcFilePath, &fileStat) == 0 && S_ISFIFO(fileStat.st_mode))
            {
                srcFileFd = open(srcFilePath, O_RDONLY | O_NONBLOCK);
            }
            else
            {
                srcFileFd = open(srcFilePath, O_RDONLY);
            }

            if (srcFileFd == -1)
            {
                perror("Error opening source file\n");
                return;
            }

            // Open/create destination file for writing
            destFileFd = open(destFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (destFileFd == -1)
            {
                perror("Error opening destination file");
                close(srcFileFd);
                return;
            }
            Task t;
            snprintf(t.srcFilePathTask, PATH_MAX, "%s", srcFilePath);
            snprintf(t.destFilePathTask, PATH_MAX, "%s", destFilePath);
            t.srcFd = srcFileFd;
            t.destFd = destFileFd;

            while (taskCount == bufferSize) /* block if buffer is full */
                pthread_cond_wait(&prodQueue, &mutexQueue);

            taskQueue[taskCount] = t;
            taskCount++;
            printf("Increased Pool Count. Active Pool Count = %d\n", taskCount);
            pthread_mutex_unlock(&mutexQueue);
            pthread_cond_signal(&condQueue);
        }
        else if (S_ISFIFO(fileStat.st_mode))
        {
            totalFIFO++;
            if (stat(srcFilePath, &fileStat) == 0 && S_ISFIFO(fileStat.st_mode))
            {
                // Check if the FIFO file exists
                if (access(destFilePath, F_OK) == 0)
                {
                    // Remove the existing FIFO
                    if (unlink(destFilePath) != 0)
                    {
                        printf("Error removing the FIFO.\n");
                        continue;
                    }
                }

                // Entry is a fifo
                if (mkfifo(destFilePath, fileStat.st_mode) != 0)
                {
                    perror("Error copy FIFO file\n");
                    close(srcFileFd);
                    continue;
                }
            }
        }
    }
    // Close source directory
    closedir(srcDir);
}

void executeTask(Task *task)
{
    copyFile(task->srcFd, task->destFd);
}

void copyFile(const int srcFd, const int destFd)
{
    ssize_t bytesRead;
    char buffer[4096]; // Buffer for reading/writing data

    // Copy file contents
    printf("Copy pf = %d \n",srcFd);
    while ((bytesRead = read(srcFd, buffer, sizeof(buffer))) > 0)
    {
        if (write(destFd, buffer, bytesRead) != bytesRead)
        {
            perror("Error writing to destination file");
            break;
        }
        totalBytes += bytesRead;
    }

    // Close file descriptors
    close(srcFd);
    close(destFd);
}

void handle_signal(int sg)
{
    if (sg == SIGINT)
    {
        printf("Close Signal is come. Exit.\n");
    }
    else if (sg == SIGTERM)
    {
        printf("Close Signal is come. Exit.\n");
    }

    // // Notify consumers that producer is done
    // pthread_mutex_lock(&bufferMutex);
    // isProducerDone = 1;
    // pthread_mutex_unlock(&bufferMutex);
    // pthread_cond_broadcast(&full);

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    pthread_cond_destroy(&prodQueue);

    exit(0);
}

// Function to get the current time in milliseconds
long long current_timestamp()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}