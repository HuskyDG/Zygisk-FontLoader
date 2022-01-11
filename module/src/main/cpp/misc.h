#ifndef MISC_H
#define MISC_H

int read_full(int fd, void *buf, size_t count);

int write_full(int fd, const void *buf, size_t count);

int read_int(int fd);

void write_int(int fd, int val);

#endif // MISC_H
