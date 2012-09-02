#ifndef _EXECFS_LOG_H_
#define _EXECFS_LOG_H_

int log_open(char *filename);
void log_close(void);
void log_write(char *format, ...);

extern FILE *log_file;

#endif
