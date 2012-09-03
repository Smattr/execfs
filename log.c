/* Logging functionality. Mainly useful for debugging. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "globals.h"

FILE *log_file = NULL;

int log_open(char *filename) {
    if (log_file != NULL) {
        fclose(log_file);
    }
    log_file = fopen(filename, "a");
    return (log_file == NULL);
}

void log_close(void) {
    fclose(log_file);
    log_file = NULL;
}

void log_write(char *format, ...) {
    if (log_file != NULL) {
        /* Print timestamp */
        time_t tt = time(NULL);
        struct tm *t = localtime(&tt);
        fprintf(log_file, "[%02d-%02d-%04d %02d:%02d:%02d] ", t->tm_mday,
            t->tm_mon, t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec);

        va_list ap;
        va_start(ap, format);
        (void)vfprintf(log_file, format, ap);
        va_end(ap);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}
