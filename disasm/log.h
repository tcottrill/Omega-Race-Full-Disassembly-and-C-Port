#ifndef LOG_H
#define LOG_H

#include "stdio.h"


int open_log(char *filename);
int wrlog(char *format, ...);
int close_log(void);

#endif