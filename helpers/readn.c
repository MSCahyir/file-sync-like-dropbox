/* include readn */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

/* Read "n" bytes from a descriptor. */
ssize_t readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return (-1);
        }
        else if (nread == 0)
            break; /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

ssize_t Readn(int fd, void *ptr, size_t nbytes)
{
    ssize_t n;

    if ((n = readn(fd, ptr, nbytes)) < 0)
        perror("readn error");
    return (n);
}
