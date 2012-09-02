#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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
        va_list ap;
        va_start(ap, format);
        (void)vfprintf(log_file, format, ap);
        va_end(ap);
    }
}
