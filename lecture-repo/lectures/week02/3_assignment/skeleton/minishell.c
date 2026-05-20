/*
 * minishell.c - 간단한 셸 구현
 *
 * 지원 기능:
 *   - 단일 명령어 실행: ls -l
 *   - 파이프: cmd1 | cmd2
 *   - 입력 리다이렉션: cmd < file
 *   - 출력 리다이렉션: cmd > file
 *   - 종료: exit
 *
 * Compile: gcc -Wall -o minishell minishell.c
 * Run:     ./minishell
 *
 * 파싱은 이미 구현되어 있습니다. 학생은 다음 함수만 구현하면 됩니다:
 *   1. execute_command()  - 단일 명령어 실행 (리다이렉션 포함)
 *   2. execute_pipe()     - 파이프로 연결된 두 명령어 실행
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE    1024    /* max input line length */
#define MAX_ARGS    64      /* max number of arguments */

/* ===== Command structure ===== */

struct command {
    char *argv[MAX_ARGS];   /* argument list (NULL-terminated) */
    char *infile;           /* input redirection filename, or NULL */
    char *outfile;          /* output redirection filename, or NULL */
};

/* ===== Parsing (already implemented) ===== */

/*
 * Trim leading and trailing whitespace in-place.
 * Returns pointer to trimmed string (within the same buffer).
 */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '\0')
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n'))
        *end-- = '\0';
    return s;
}

/*
 * Parse a single command segment (no pipe) into a struct command.
 * Handles < and > redirections.
 *
 * Example: "sort < input.txt" → argv={"sort", NULL}, infile="input.txt"
 * Example: "ls -l > out.txt"  → argv={"ls", "-l", NULL}, outfile="out.txt"
 */
static int parse_command(char *line, struct command *cmd)
{
    memset(cmd, 0, sizeof(*cmd));

    int argc = 0;
    char *token = strtok(line, " \t");

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            /* input redirection */
            token = strtok(NULL, " \t");
            if (token == NULL) {
                fprintf(stderr, "minishell: syntax error near '<'\n");
                return -1;
            }
            cmd->infile = token;
        } else if (strcmp(token, ">") == 0) {
            /* output redirection */
            token = strtok(NULL, " \t");
            if (token == NULL) {
                fprintf(stderr, "minishell: syntax error near '>'\n");
                return -1;
            }
            cmd->outfile = token;
        } else {
            if (argc >= MAX_ARGS - 1) {
                fprintf(stderr, "minishell: too many arguments\n");
                return -1;
            }
            cmd->argv[argc++] = token;
        }
        token = strtok(NULL, " \t");
    }

    cmd->argv[argc] = NULL;

    if (argc == 0) {
        return -1;  /* empty command */
    }

    return 0;
}

/* ===== Student implementation area ===== */

/*
 * execute_command - 단일 명령어를 실행합니다.
 *
 * 이 함수는 다음을 수행해야 합니다:
 *   1. fork()로 자식 프로세스 생성
 *   2. 자식에서:
 *      a. cmd->infile이 있으면 입력 리다이렉션 설정 (stdin을 파일로)
 *      b. cmd->outfile이 있으면 출력 리다이렉션 설정 (stdout을 파일로)
 *      c. execvp()로 명령어 실행
 *      d. exec 실패 시 에러 출력 후 exit(127)
 *   3. 부모에서: wait()으로 자식 종료 대기
 *
 * 매개변수:
 *   cmd - 파싱된 명령어 구조체 (argv, infile, outfile 포함)
 *
 * 반환값:
 *   자식의 exit status (WEXITSTATUS), 또는 에러 시 -1
 */
static int execute_command(struct command *cmd)
{
    /* TODO 1: fork()로 자식 프로세스를 생성하세요.
     *   - fork() 실패 시 perror 후 return -1
     */

    /* TODO 2 (자식 프로세스):
     *   a. cmd->infile이 NULL이 아니면:
     *      - open(cmd->infile, O_RDONLY)로 파일 열기
     *      - 실패 시 perror 후 exit(1)
     *      - dup2(fd, STDIN_FILENO)로 stdin 교체
     *      - close(fd)로 원본 fd 정리
     *
     *   b. cmd->outfile이 NULL이 아니면:
     *      - open(cmd->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0644)로 파일 열기
     *      - 실패 시 perror 후 exit(1)
     *      - dup2(fd, STDOUT_FILENO)로 stdout 교체
     *      - close(fd)로 원본 fd 정리
     *
     *   c. execvp(cmd->argv[0], cmd->argv)로 명령어 실행
     *
     *   d. execvp가 반환했으면 실패한 것:
     *      - perror("minishell") 출력
     *      - exit(127)
     */

    /* TODO 3 (부모 프로세스):
     *   - int status;
     *   - waitpid(pid, &status, 0)로 자식 종료 대기
     *   - WIFEXITED(status)이면 WEXITSTATUS(status) 반환
     *   - 그렇지 않으면 -1 반환
     */

    return -1;  /* placeholder */
}

/*
 * execute_pipe - 파이프로 연결된 두 명령어를 실행합니다.
 *
 * 셸의 "cmd1 | cmd2" 동작을 구현합니다:
 *   - cmd1의 stdout → pipe → cmd2의 stdin
 *
 * 이 함수는 다음을 수행해야 합니다:
 *   1. pipe()로 파이프 생성
 *   2. fork()로 자식1 생성 (cmd1 실행)
 *      - stdout을 pipe의 쓰기 끝으로 dup2
 *      - cmd1->infile이 있으면 입력 리다이렉션도 설정
 *      - execvp로 cmd1 실행
 *   3. fork()로 자식2 생성 (cmd2 실행)
 *      - stdin을 pipe의 읽기 끝으로 dup2
 *      - cmd2->outfile이 있으면 출력 리다이렉션도 설정
 *      - execvp로 cmd2 실행
 *   4. 부모에서 pipe 양쪽 닫기 + 자식 2개 모두 wait
 *
 * 매개변수:
 *   cmd1 - 파이프 왼쪽 명령어
 *   cmd2 - 파이프 오른쪽 명령어
 *
 * 반환값:
 *   cmd2의 exit status (WEXITSTATUS), 또는 에러 시 -1
 */
static int execute_pipe(struct command *cmd1, struct command *cmd2)
{
    /* TODO 4: pipe()로 파이프를 생성하세요.
     *   int fd[2];
     *   pipe(fd) 호출, 실패 시 perror 후 return -1
     */

    /* TODO 5: fork()로 자식1을 생성하세요 (cmd1 실행).
     *   자식1에서:
     *     - pipe의 읽기 끝(fd[0]) 닫기
     *     - dup2(fd[1], STDOUT_FILENO)로 stdout을 pipe 쓰기 끝으로
     *     - close(fd[1])
     *     - cmd1->infile이 있으면 입력 리다이렉션 설정
     *     - execvp(cmd1->argv[0], cmd1->argv)
     *     - exec 실패 시 perror 후 exit(127)
     */

    /* TODO 6: fork()로 자식2를 생성하세요 (cmd2 실행).
     *   자식2에서:
     *     - pipe의 쓰기 끝(fd[1]) 닫기
     *     - dup2(fd[0], STDIN_FILENO)로 stdin을 pipe 읽기 끝으로
     *     - close(fd[0])
     *     - cmd2->outfile이 있으면 출력 리다이렉션 설정
     *     - execvp(cmd2->argv[0], cmd2->argv)
     *     - exec 실패 시 perror 후 exit(127)
     */

    /* TODO 7: 부모에서 정리하세요.
     *   - pipe의 양쪽 끝(fd[0], fd[1]) 모두 닫기
     *   - waitpid로 자식1 종료 대기
     *   - waitpid로 자식2 종료 대기
     *   - cmd2의 exit status 반환
     */

    return -1;  /* placeholder */
}

/* ===== Main loop (already implemented) ===== */

int main(void)
{
    char line[MAX_LINE];

    while (1) {
        /* print prompt */
        printf("minishell> ");
        fflush(stdout);

        /* read input */
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;  /* EOF (Ctrl-D) */
        }

        /* trim whitespace */
        char *input = trim(line);
        if (*input == '\0')
            continue;  /* empty line */

        /* handle built-in: exit */
        if (strcmp(input, "exit") == 0)
            break;

        /* check for pipe */
        char *pipe_pos = strchr(input, '|');

        if (pipe_pos != NULL) {
            /* split at pipe character */
            *pipe_pos = '\0';
            char *left = input;
            char *right = pipe_pos + 1;

            /* make copies for strtok (strtok modifies the string) */
            char left_copy[MAX_LINE], right_copy[MAX_LINE];
            strncpy(left_copy, left, sizeof(left_copy) - 1);
            left_copy[sizeof(left_copy) - 1] = '\0';
            strncpy(right_copy, right, sizeof(right_copy) - 1);
            right_copy[sizeof(right_copy) - 1] = '\0';

            struct command cmd1, cmd2;
            if (parse_command(left_copy, &cmd1) < 0)
                continue;
            if (parse_command(right_copy, &cmd2) < 0)
                continue;

            execute_pipe(&cmd1, &cmd2);
        } else {
            /* single command */
            char copy[MAX_LINE];
            strncpy(copy, input, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';

            struct command cmd;
            if (parse_command(copy, &cmd) < 0)
                continue;

            execute_command(&cmd);
        }
    }

    return 0;
}
