#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <semaphore.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LOCKS 100

typedef struct {
    char filepath[PATH_MAX];
    int readers_count;
    sem_t mutex;      // Protects readers_count
    sem_t write_sem;  // Blocks writers (and readers if writer active)
    int usage_count;  // Reference count for this slot
} FileLock;

typedef struct {
    FileLock locks[MAX_LOCKS];
    sem_t global_lock; // Protects the locks array allocation
} SharedState;

void init_shared_memory();
FileLock* get_file_lock(const char* path);
void release_file_lock(FileLock* lock);
void reader_lock(FileLock* lock);
void reader_unlock(FileLock* lock);
void writer_lock(FileLock* lock);
void writer_unlock(FileLock* lock);

#endif // CONCURRENCY_H
