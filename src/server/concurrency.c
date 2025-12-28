#include "concurrency.h"

static SharedState *shared_state = NULL;

/*
 * Initializes shared memory segment using mmap and sets up
 * global and per-file semaphores for concurrency control.
 */
void init_shared_memory() {
    // Create shared memory mapping
    shared_state = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_state == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // Initialize global lock
    if (sem_init(&shared_state->global_lock, 1, 1) == -1) {
        perror("sem_init global_lock failed");
        exit(EXIT_FAILURE);
    }

    // Initialize all locks
    for (int i = 0; i < MAX_LOCKS; i++) {
        shared_state->locks[i].usage_count = 0;
        shared_state->locks[i].readers_count = 0;
        // Initialize semaphores as process-shared (2nd arg = 1)
        if (sem_init(&shared_state->locks[i].mutex, 1, 1) == -1) {
            perror("sem_init mutex failed");
            exit(EXIT_FAILURE);
        }
        if (sem_init(&shared_state->locks[i].write_sem, 1, 1) == -1) {
            perror("sem_init write_sem failed");
            exit(EXIT_FAILURE);
        }
    }
    printf("Shared memory initialized for concurrency control.\n");
}

/*
 * Retrieves or creates a FileLock structure for a given path.
 * Uses a global semaphore to ensure thread-safe access to the lock array.
 */
FileLock* get_file_lock(const char* path) {
    if (shared_state == NULL) {
        fprintf(stderr, "Shared memory not initialized!\n");
        return NULL;
    }

    sem_wait(&shared_state->global_lock);

    // Check if lock already exists for this path
    for (int i = 0; i < MAX_LOCKS; i++) {
        if (shared_state->locks[i].usage_count > 0 && 
            strncmp(shared_state->locks[i].filepath, path, PATH_MAX) == 0) {
            shared_state->locks[i].usage_count++;
            sem_post(&shared_state->global_lock);
            return &shared_state->locks[i];
        }
    }

    // Find a free slot
    for (int i = 0; i < MAX_LOCKS; i++) {
        if (shared_state->locks[i].usage_count == 0) {
            strncpy(shared_state->locks[i].filepath, path, PATH_MAX - 1);
            shared_state->locks[i].filepath[PATH_MAX - 1] = '\0';
            shared_state->locks[i].usage_count = 1;
            shared_state->locks[i].readers_count = 0;
            
            // Reset semaphores
            sem_destroy(&shared_state->locks[i].mutex);
            sem_destroy(&shared_state->locks[i].write_sem);
            sem_init(&shared_state->locks[i].mutex, 1, 1);
            sem_init(&shared_state->locks[i].write_sem, 1, 1);

            sem_post(&shared_state->global_lock);
            return &shared_state->locks[i];
        }
    }

    sem_post(&shared_state->global_lock);
    fprintf(stderr, "Error: No more lock slots available!\n");
    return NULL;
}

/*
 * Releases a file lock.
 * Decreases the usage count and clears the path if the last user releases the lock.
 */
void release_file_lock(FileLock* lock) {
    if (lock == NULL || shared_state == NULL) return;

    sem_wait(&shared_state->global_lock);
    
    if (lock->usage_count > 0) {
        lock->usage_count--;
        if (lock->usage_count == 0) {
            memset(lock->filepath, 0, PATH_MAX);
        }
    }

    sem_post(&shared_state->global_lock);
}

/*
 * Acquires a read lock. Multiple readers can hold the lock simultaneously,
 * but the first reader blocks any writers.
 */
void reader_lock(FileLock* lock) {
    if (lock == NULL) return;

    int sem_value;
    sem_getvalue(&lock->write_sem, &sem_value);
    if (sem_value == 0) {
        send_string("waiting to read...");
    }

    sem_wait(&lock->mutex);
    lock->readers_count++;
    if (lock->readers_count == 1) {
        sem_wait(&lock->write_sem); // First reader blocks writers
    }
    sem_post(&lock->mutex);
}

/*
 * Releases a read lock.
 * Decreases the reader count and releases the write semaphore if no readers remain.
 */
void reader_unlock(FileLock* lock) {
    if (lock == NULL) return;

    sem_wait(&lock->mutex);
    lock->readers_count--;
    if (lock->readers_count == 0) {
        sem_post(&lock->write_sem); // Last reader releases writers
    }
    sem_post(&lock->mutex);
}

/*
 * Acquires a write lock. Blocks all other readers and writers.
 */
void writer_lock(FileLock* lock) {
    if (lock == NULL) return;

    int sem_value;
    sem_getvalue(&lock->write_sem, &sem_value);
    if (sem_value == 0) {
        send_string("waiting to write...");
    }

    sem_wait(&lock->write_sem);
}

/*
 * Releases a write lock.
 * Unblocks all readers and writers.
 */
void writer_unlock(FileLock* lock) {
    if (lock == NULL) return;
    sem_post(&lock->write_sem);
}
