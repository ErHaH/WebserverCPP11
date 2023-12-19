#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    int fd = open("a.cpp", O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    char buf1[1024] = {0};
    char buf2[1024] = {0};

    struct iovec iov[2];
    iov[0].iov_base = buf1;
    iov[0].iov_len = sizeof(buf1);
    iov[1].iov_base = buf2;
    iov[1].iov_len = sizeof(buf2);

    ssize_t nwritten = readv(fd, iov, 2);
    if (nwritten == -1) {
        perror("writev");
        return -1;
    }

    printf("Wrote %s bytes\n", buf1);
    printf("Wrote %s bytes\n", buf2);


    close(fd);

    return 0;
}
