/*
 * Copyright(c) 2023-2024 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_LINE 80             /* 명령어의 최대 길이 */

static void IOcon(char *infile, char *outfile, char *argv[]) {
    if (infile) {
        int fd = open(infile, O_RDONLY);
        if(fd < 0) {
            perror("file open failed.");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (outfile) {
        int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if(fd < 0) {
            perror("file open error.");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    execvp(argv[0], argv);
}

/*
 * cmdexec - 명령어를 파싱해서 실행한다.
 * 스페이스와 탭을 공백문자로 간주하고, 연속된 공백문자는 하나의 공백문자로 축소한다. 
 * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
 * 기호 '<' 또는 '>'를 사용하여 표준 입출력을 파일로 바꾸거나,
 * 기호 '|'를 사용하여 파이프 명령을 실행하는 것도 여기에서 처리한다.
 */
static void cmdexec(char *cmd)
{
    char *argv[MAX_LINE/2+1];   /* 명령어 인자를 저장하기 위한 배열 */
    int argc = 0;               /* 인자의 개수 */
    char *p, *q;                /* 명령어를 파싱하기 위한 변수 */
    char *infile = NULL, *outfile = NULL;       /* variable to store file names */
    bool ispipe = false;          /* variable to check pipe */
    int pipefd[2];             /* variable to store pipe file descriptors */

    /*
     * 명령어 앞부분 공백문자를 제거하고 인자를 하나씩 꺼내서 argv에 차례로 저장한다.
     * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
     */
    p = cmd; p += strspn(p, " \t"); //frontal empty space removal / strspn returns the length of line before it meets the " \t".
    do {
        /*
         * 공백문자, 큰 따옴표, 작은 따옴표가 있는지 검사한다.
         */ 
        q = strpbrk(p, " \t\'\"<>|"); //returns pointer of first one that matches the " \t\'\"".
        /*
	     * 공백문자가 있거나 아무 것도 없으면 공백문자까지 또는 전체를 하나의 인자로 처리한다.
         */ 
        if (q == NULL || *q == ' ' || *q == '\t') {
            q = strsep(&p, " \t"); //returns entire set of characters brfore meets the " \t", and &p points the character after " " or "\t".
            if (*q) argv[argc++] = q;
        }
        /*
         * if q is < then adds next word into the input file infile.
         */
        else if(*q == '<') {
            strsep(&p, "<");
            while (*p == ' ' || *p == '\t') p++; //remove the empty space after the "<".
            infile = strsep(&p, " \t\0");
        }
        /*
         * if q is > then adds next word into the output file outfile.
         */
        else if(*q == '>') {
            strsep(&p, ">");
            while(*p == ' ' || *p == '\t') p++; //remove the empty space after the ">".
            outfile = strsep(&p, " \t\0");
        }
        /*
         * if q is | when meets this the work have to be done recursivly.
         * so, the command before the | is executed and the output is used as the input of the next command.
         * here, implement pipe checker.
         * implementing pipe function will be done in the execution part.
         */
        else if(*q == '|') {
            ispipe = true;
            strsep(&p, "|");
            break;
        }
        /*
         * 작은 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 작은 따옴표 위치에서 두 번째 작은 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 작은 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else if (*q == '\'') {
            q = strsep(&p, "\'"); //q is element of commandline before "\'".
            if (*q) argv[argc++] = q; //put that q into the argv elements.
            q = strsep(&p, "\'"); //when p meets second "\'", it parsings the p.
            if (*q) argv[argc++] = q; //add the element inside the two "\'"s to the argv.
        }
        /*
         * 큰 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 큰 따옴표 위치에서 두 번째 큰 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 큰 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else { //same with "\'" line process.
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
        }

    } while (p); //for the entire command that has been in the input command line.
    argv[argc] = NULL; //competablity for the execve().
    
    /*
     * argv에 저장된 명령어를 실행한다.
     */
    if (argc > 0){
        if (!ispipe) {
            IOcon(infile, outfile, argv);
        }
        // if (fork() == 0) { //this child process partition is necessary to avoid execution of first pipe command.
        else {
            pipe(pipefd);
            if(fork() == 0) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                
                IOcon(infile, outfile, argv);
                exit(EXIT_SUCCESS);
            }
            if(fork() == 0) {
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                
                cmdexec(p);
                exit(EXIT_SUCCESS);
            }
            close(pipefd[1]);
            close(pipefd[0]); //thsi was the problem. I didn't close the pipefd[0] and pipefd[1] after the pipe command.
            wait(NULL); // adding another wait to figure this works.
            // exit(EXIT_SUCCESS); //restoreing this waiting process didn't solved the problem.
        }
        //wait(NULL);
    }
}
/*
 * 기능이 간단한 유닉스 셸인 tsh (tiny shell)의 메인 함수이다.
 * tsh은 프로세스 생성과 파이프를 통한 프로세스간 통신을 학습하기 위한 것으로
 * 백그라운드 실행, 파이프 명령, 표준 입출력 리다이렉션 일부만 지원한다.
 */
int main(void)
{
    char cmd[MAX_LINE+1];       /* 명령어를 저장하기 위한 버퍼 */
    int len;                    /* 입력된 명령어의 길이 */
    pid_t pid;                  /* 자식 프로세스 아이디 */
    bool background;            /* 백그라운드 실행 유무 */
    
    /*
     * 종료 명령인 "exit"이 입력될 때까지 루프를 무한 반복한다.
     */
    while (true) {
        /*
         * 좀비 (자식)프로세스가 있으면 제거한다.
         */
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);
        /*
         * 셸 프롬프트를 출력한다. 지연 출력을 방지하기 위해 출력버퍼를 강제로 비운다.
         */
        printf("tsh> "); fflush(stdout);//fflush: empty the output buffer so that tsh> is show right away.
        /*
         * 표준 입력장치로부터 최대 MAX_LINE까지 명령어를 입력 받는다.
         * 입력된 명령어 끝에 있는 새줄문자를 널문자로 바꿔 C 문자열로 만든다.
         * 입력된 값이 없으면 새 명령어를 받기 위해 루프의 처음으로 간다.
         */
        len = read(STDIN_FILENO, cmd, MAX_LINE);
        if (len == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        cmd[--len] = '\0';
        if (len == 0)
            continue;
        /*
         * 종료 명령이면 루프를 빠져나간다.
         */
        if(!strcasecmp(cmd, "exit")) //strcasecmp: compare cmd and "exit" whether they are capital letter or not.
            break;
        /*
         * 백그라운드 명령인지 확인하고, '&' 기호를 삭제한다.
         */
        char *p = strchr(cmd, '&');
        if (p != NULL) {
            background = true;
            *p = '\0';
        }
        else
            background = false;
        /*
         * 자식 프로세스를 생성하여 입력된 명령어를 실행하게 한다.
         */
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         */
        else if (pid == 0) {
            cmdexec(cmd);
            exit(EXIT_SUCCESS);
        }
        /*
         * 포그라운드 실행이면 부모 프로세스는 자식이 끝날 때까지 기다린다.
         * 백그라운드 실행이면 기다리지 않고 다음 명령어를 입력받기 위해 루프의 처음으로 간다.
         */
	    else if (!background)
            waitpid(pid, NULL, 0);
    }
    return 0;
}
