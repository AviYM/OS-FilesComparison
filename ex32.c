// Avi Miletzky

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wait.h>

#define BUFFER_SIZE 151
#define CONFIG_ROW 3
#define ERROR -1
#define S_C_ERROR "Error in system call"
#define TEMP_OUTPUT "output.txt"
#define COMP_PROG "./comp.out"
#define RESULT_FILE "results.csv"
#define TEMP_EXEC_F "e.out"

#define NO_C_FILE 0
#define COMPILATION_ERROR 20
#define TIMEOUT 40
#define BAD_OUTPUT 60
#define SIMILAR_OUTPUT 80
#define GREAT_JOB 100

void errorsManager(char *error) {
    write(STDERR_FILENO, error, strlen(error));
}

char *getStatusByScore(int score) {
    switch (score) {
        case GREAT_JOB: return ",100,GREAT_JOB\n";
        case SIMILAR_OUTPUT: return ",80,SIMILAR_OUTPUT\n";
        case BAD_OUTPUT: return ",60,BAD_OUTPUT\n";
        case TIMEOUT: return ",40,TIMEOUT\n";
        case COMPILATION_ERROR: return ",20,COMPILATION_ERROR\n";
        case NO_C_FILE: return ",0,NO_C_FILE\n";
        default: errorsManager("Invalid score\n");
    }
}

int saveStatusToFile(int resFd, char *name, char *status) {
    char line[MAX_INPUT];
    bzero(line, MAX_INPUT);

    strcpy(line, name);
    strcat(line, status);
    if (write(resFd, line, strlen(line)) == ERROR) {
        errorsManager(S_C_ERROR);
        return ERROR;
    }
    return 0;
}

int runsFile(char **arguments, int inP, int outP) {
    pid_t pidChild;
    int status;

    pidChild = fork();
    if (pidChild > 0) {
        int isFinish;
        sleep(5);
        isFinish = waitpid(pidChild, &status, WNOHANG);
        if (isFinish) {
            return WEXITSTATUS(status);
        } else {
            kill(pidChild, SIGKILL);
            return TIMEOUT;
        }
    }
    if (pidChild == 0) {
        dup2(inP, STDIN_FILENO);
        dup2(outP, STDOUT_FILENO);
        execvp(arguments[0], arguments);
    }
    errorsManager(S_C_ERROR);
    return ERROR;
}

int runsCommand(char **arguments) {
    pid_t pidChild;
    int status;

    pidChild = fork();
    if (pidChild > 0) {
        waitpid(pidChild, &status, WUNTRACED);
        return WEXITSTATUS(status);
    }
    if (pidChild == 0) {
        execvp(arguments[0], arguments);
    }
    errorsManager(S_C_ERROR);
    return ERROR;
}

int runsAndCompResult(char *cFilePath,
                      char configData[CONFIG_ROW][BUFFER_SIZE]) {

    char *compileCmnd[5] = {"gcc", cFilePath, "-o", TEMP_EXEC_F, NULL};
    char path[MAX_INPUT] = "./";
    strcat(path, TEMP_EXEC_F);
    char *runCmnd[2] = {path, NULL};
    char *compareParams[4] = {COMP_PROG, configData[2], TEMP_OUTPUT, NULL};
    int ret, inP, outP;

    if ((ret = runsCommand(compileCmnd)) == ERROR) {
        return ERROR;
    }
    if (ret != 0) {
        return COMPILATION_ERROR;
    }

    if ((inP = open(configData[1], O_RDONLY)) == ERROR) {
        unlink(TEMP_EXEC_F);
        errorsManager(S_C_ERROR);
        return ERROR;
    }
    if ((outP = open(TEMP_OUTPUT, O_RDWR | O_CREAT, 0644)) == ERROR) {
        unlink(TEMP_EXEC_F);
        close(inP);
        errorsManager(S_C_ERROR);
        return ERROR;
    }

    ret = runsFile(runCmnd, inP, outP);
    unlink(TEMP_EXEC_F);
    close(inP);
    if (ret == ERROR) {
        close(outP);
        unlink(TEMP_OUTPUT);
        return ERROR;
    }
    if (ret == TIMEOUT) {
        close(outP);
        unlink(TEMP_OUTPUT);
        return TIMEOUT;
    }

    ret = runsCommand(compareParams);
    close(outP);
    unlink(TEMP_OUTPUT);

    if (ret == ERROR) return ERROR;
    if (ret == 1) return GREAT_JOB;
    if (ret == 2) return BAD_OUTPUT;
    if (ret == 3) return SIMILAR_OUTPUT;
}

bool isCFile(char *fileName) {
    int i;
    for (i = strlen(fileName) - 1; i >= 0; --i) {
        if (fileName[i] == '\0') continue;
        if ((fileName[i] == 'c') && (fileName[i - 1] == '.')) return true;
        else return false;
    }
}

int recursiveSubDir(char dirPath[BUFFER_SIZE], char cFilePath[BUFFER_SIZE]) {
    DIR *dd;
    struct dirent *dir;
    int ret = 0;
    char currPath[MAX_INPUT];

    if ((dd = opendir(dirPath)) == NULL) {
        errorsManager(S_C_ERROR);
        return ERROR;
    }

    while ((dir = readdir(dd)) != NULL) {
        if (dir->d_type != DT_DIR) {
            if (isCFile(dir->d_name)) {
                strcpy(cFilePath, dirPath);
                strcat(cFilePath, "/");
                strcat(cFilePath, dir->d_name);
                ret = 1;
                break;
            }
        } else {
            if (!strcmp(dir->d_name, ".") ||
                !strcmp(dir->d_name, "..")) continue;
            bzero(currPath, MAX_INPUT);
            strcpy(currPath, dirPath);
            strcat(currPath, "/");
            strcat(currPath, dir->d_name);
            ret = recursiveSubDir(currPath, cFilePath);
            if (ret) break;
        }
    }
    closedir(dd);
    return ret;
}

int mainDirIter(char configData[CONFIG_ROW][BUFFER_SIZE]) {
    DIR *dd;
    struct dirent *dir;
    char cFilePath[BUFFER_SIZE], dirPath[BUFFER_SIZE];
    char *status = "", **params;
    int resFd, score, result = 0;

    if ((dd = opendir(configData[0])) == NULL) {
        errorsManager(S_C_ERROR);
        return ERROR;
    }

    resFd = open(RESULT_FILE, O_WRONLY | O_TRUNC | O_APPEND | S_IRWXG);
    if (resFd == ERROR) {
        resFd = open(RESULT_FILE, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (resFd == ERROR) {
            closedir(dd);
            errorsManager(S_C_ERROR);
            return ERROR;
        }
    }

    while ((dir = readdir(dd)) != NULL) {
        if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")
            || dir->d_type != DT_DIR)
            continue;

        bzero(cFilePath, BUFFER_SIZE);
        bzero(dirPath, BUFFER_SIZE);
        strcpy(dirPath, configData[0]);
        strcat(dirPath, "/");
        strcat(dirPath, dir->d_name);
        result = recursiveSubDir(dirPath, cFilePath);
        if (result == ERROR) break;
        if (!result) {
            score = NO_C_FILE;
            status = getStatusByScore(score);
        } else {
            result = runsAndCompResult(cFilePath, configData);
            if (result == ERROR) break;
            score = result;
            status = getStatusByScore(score);
        }

        if ((result = saveStatusToFile(resFd, dir->d_name, status)) == ERROR)
            break;
    }
    close(resFd);
    closedir(dd);
    return result;
}

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE * CONFIG_ROW];
    char *separatedArgs;
    char configData[CONFIG_ROW][BUFFER_SIZE];
    int readConfig, fdConfig, i = 0, ret;

    if (argc != 2) {
        errorsManager("The number of parameters is different than 2\n");
        return ERROR;
    }

    fdConfig = open(argv[1], O_RDONLY);
    if (fdConfig < 0) {
        errorsManager(S_C_ERROR);
        return ERROR;
    }

    bzero(buffer, (BUFFER_SIZE * CONFIG_ROW));
    readConfig = read(fdConfig, buffer, (BUFFER_SIZE * CONFIG_ROW));
    close(fdConfig);
    if (readConfig == -1) {
        errorsManager(S_C_ERROR);
        return ERROR;
    }

    separatedArgs = strtok(buffer, "\n");
    while (separatedArgs != NULL && i < CONFIG_ROW) {
        strcpy(configData[i], separatedArgs);
        i++;
        separatedArgs = strtok(NULL, "\n");
    }

    ret = mainDirIter(configData);

    return ret;
}
