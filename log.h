#ifndef _EXECFS_LOG_H_
#define _EXECFS_LOG_H_

int log_open(char *filename);
void log_close(void);
void log_write(char *format, ...);

/* Swap the comments below to switch log information between standard logging
 * and code debugging.
 */
#define LOG(format, args...) \
    log_write("%s:%d:%s(): " format, __FILE__, __LINE__, __func__ , ## args)
//#define LOG(format, args...) log_write(format , ## args)

extern FILE *log_file;

#endif
