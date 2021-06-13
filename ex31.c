// Avi Miletzky

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 10
#define IDENTICAL 1
#define SIMILAR 3
#define DIFFERENT 2
#define ERROR -1


void errorsManager(char *error) {
    write(STDERR_FILENO, error, strlen(error));
}

int checkTheFileEnd(int fd, char buffer[], int pos) {
    int readF = 1;
    while (readF) {
        if (pos == -1) {
            bzero(buffer, BUFFER_SIZE);
            readF = read(fd, buffer, BUFFER_SIZE);
            if (readF == -1) {
                errorsManager("failed reading file\n");
                return ERROR;
            }
            pos++;
        }

        if (buffer[pos] == '\0') {
            readF = 0;
            continue;
        }

        if (buffer[pos] != ' ' && buffer[pos] != '\n') {
            return DIFFERENT;
        }
        pos++;
        if (pos == BUFFER_SIZE) {
            pos = -1;
        }
    }
    return SIMILAR;
}

int filesComparison(int fd1, int fd2) {
    int status = IDENTICAL, readF1 = 1, readF2 = 1, posB1 = -1, posB2 = -1, distance;
    char buf1[BUFFER_SIZE], buf2[BUFFER_SIZE];

    while (readF1 && readF2) {
        //reads files to buffers
        if (posB1 == -1) {
            bzero(buf1, BUFFER_SIZE);
            readF1 = read(fd1, buf1, BUFFER_SIZE);
            posB1++;
        }
        if (posB2 == -1) {
            bzero(buf2, BUFFER_SIZE);
            readF2 = read(fd2, buf2, BUFFER_SIZE);
            posB2++;
        }
        if (readF1 == -1 || readF2 == -1) {
            errorsManager("failed reading file\n");
            return ERROR;
        }

        // if reads file is finished, has to get out from the loop.
        if (buf1[posB1] == '\0') {
            readF1 = 0;
        }
        if (buf2[posB2] == '\0') {
            readF2 = 0;
        }

        // compare buffers' characters
        distance = abs(buf1[posB1] - buf2[posB2]);
        if (distance != 0) {
            status = SIMILAR;
        }
        if (distance == 0 || distance == 32) {//() && readF1 && readF2
            posB1++;
            posB2++;
        } else {
            if (buf1[posB1] == ' ' || buf1[posB1] == '\n') posB1++;
            else if (buf2[posB2] == ' ' || buf2[posB2] == '\n') posB2++;
            else return DIFFERENT;
        }

        // in order to fill buffer in the next iteration.
        if (posB1 == BUFFER_SIZE) {
            posB1 = -1;
        }
        if (posB2 == BUFFER_SIZE) {
            posB2 = -1;
        }
    }

    if (readF1) {
       status = checkTheFileEnd(fd1, buf1, posB1);
    }
    if (readF2) {
        status = checkTheFileEnd(fd2, buf2, posB2);
    }
    return status;
}

int main(int argc, char *argv[]) {
    int fd1, fd2, result;
    if (argc != 3) {
        errorsManager("the number of parameters is different than 3\n");
        result = ERROR;
    } else {
        fd1 = open(argv[1], O_RDONLY);
        if (fd1 < 0) {
            errorsManager("failed opening file\n");
            return ERROR;
        }
        fd2 = open(argv[2], O_RDONLY);
        if (fd2 < 0) {
            errorsManager("failed opening file\n");
            result = ERROR;
        } else {
            result = filesComparison(fd1, fd2);
            close(fd2);
        }
    close(fd1);
    }
    return result;
}