/*
 * syscalls.c - stubs for the newlib file-I/O hooks.
 *
 * The firmware never uses stdio streams: all output goes through
 * serial_write(). newlib-nano still references these from its reentrancy
 * support, and nosys.specs' versions make the linker warn ("_close is not
 * implemented and will always fail"). These say the same thing quietly.
 * Heap (_sbrk) still comes from nosys.specs, using the `end` symbol from the
 * linker script.
 */
#include <errno.h>
#include <sys/stat.h>

int _close(int fd)                          { (void)fd; errno = EBADF; return -1; }
int _read(int fd, char *p, int n)           { (void)fd; (void)p; (void)n; errno = EBADF; return -1; }
int _write(int fd, const char *p, int n)    { (void)fd; (void)p; (void)n; errno = EBADF; return -1; }
int _lseek(int fd, int off, int whence)     { (void)fd; (void)off; (void)whence; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st)         { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                         { (void)fd; return 1; }
