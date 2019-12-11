#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "poet_common_definitions.h"

static void destroy_logfile_semaphore() {
    INFO("Destroying semaphore of REPORTING log file\n");
    __get_logfile_semaphore(1);
}

sem_t *__get_logfile_semaphore(int destroy) {
    static sem_t *semaphore = NULL;
    if (semaphore == NULL) {
        semaphore = (sem_t *) (calloc(1, sizeof(sem_t)));
        assertp(semaphore != NULL);
        sem_init(semaphore, 0, 1);
        atexit(destroy_logfile_semaphore);
    }

    if (destroy && semaphore != NULL) {
        sem_destroy(semaphore);
    }

    return semaphore;
}

static void __close_log_file(void) {
    FILE *logf = __get_log_file(0);
    pid_t id = getpid();
    INFO("Closing log file of pid: %d\n", id);
    REPORT("Log file of process %d is closed at time %lu\n", id, time(NULL));
    __get_log_file(1);
}

FILE *__get_log_file(int close) {
    static FILE *outf = NULL;
    if (outf == NULL) {
        pid_t id = getpid();
        char *buffer = (char *) calloc(1, BUFFER_SIZE);
        assertp(buffer != NULL);
        sprintf(buffer, "report-%03d.log", id);
        outf = fopen(buffer, "w");

        free(buffer);
        atexit(__close_log_file);
    }

    if (close && outf != NULL) {
        fclose(outf);
        outf = NULL;
    }

    return outf;
}

#ifdef __cplusplus
};
#endif