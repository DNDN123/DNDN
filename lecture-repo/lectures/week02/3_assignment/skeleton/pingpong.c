/*
 * pingpong.c - 두 프로세스가 pipe를 통해 1바이트를 주고받는 프로그램
 *
 * xv6 교재 Ch 1 Exercise 1 기반
 *
 * 동작:
 *   1. 부모가 자식에게 1바이트("p")를 보냄 (ping)
 *   2. 자식이 바이트를 받고 출력, 부모에게 1바이트("p")를 다시 보냄 (pong)
 *   3. 부모가 바이트를 받고 출력
 *   4. 왕복 시간을 측정하여 출력
 *
 * 예상 출력 형식:
 *   <pid>: received ping
 *   <pid>: received pong
 *   Round-trip time: X.XXX us
 *
 * Compile: gcc -Wall -o pingpong pingpong.c
 * Run:     ./pingpong
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

/* Get current time in microseconds */
static long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

int main(void)
{
    /*
     * 양방향 통신을 위해 pipe 2개가 필요합니다.
     * parent_to_child: 부모 → 자식 방향
     * child_to_parent: 자식 → 부모 방향
     */
    int parent_to_child[2];
    int child_to_parent[2];

    /* TODO 1: pipe 2개를 생성하세요.
     *   - pipe(parent_to_child) 호출
     *   - pipe(child_to_parent) 호출
     *   - 에러 처리 포함 (pipe() < 0 이면 perror 후 exit)
     */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* ===== 자식 프로세스 ===== */

        /* TODO 2: 사용하지 않는 pipe fd를 닫으세요.
         *   - parent_to_child의 쓰기 끝 (자식은 여기서 읽기만 함)
         *   - child_to_parent의 읽기 끝 (자식은 여기서 쓰기만 함)
         */

        /* TODO 3: 부모로부터 1바이트를 읽으세요.
         *   - read(parent_to_child[0], &buf, 1)
         *   - 성공하면 출력: printf("%d: received ping\n", getpid());
         */

        /* TODO 4: 부모에게 1바이트를 보내세요.
         *   - write(child_to_parent[1], &buf, 1)
         */

        /* TODO 5: 사용한 pipe fd를 닫고 exit(0)으로 종료하세요. */

        exit(0);  /* placeholder - TODO 완성 후 적절한 위치로 이동 */
    } else {
        /* ===== 부모 프로세스 ===== */

        /* TODO 6: 사용하지 않는 pipe fd를 닫으세요.
         *   - parent_to_child의 읽기 끝 (부모는 여기서 쓰기만 함)
         *   - child_to_parent의 쓰기 끝 (부모는 여기서 읽기만 함)
         */

        long start_time = get_time_us();

        /* TODO 7: 자식에게 1바이트를 보내세요 (ping).
         *   - char buf = 'p';
         *   - write(parent_to_child[1], &buf, 1)
         */

        /* TODO 8: 자식으로부터 1바이트를 읽으세요 (pong).
         *   - read(child_to_parent[0], &buf, 1)
         *   - 성공하면 출력: printf("%d: received pong\n", getpid());
         */

        long end_time = get_time_us();
        double elapsed = (double)(end_time - start_time);
        printf("Round-trip time: %.3f us\n", elapsed);

        /* TODO 9: 사용한 pipe fd를 닫고 wait()으로 자식을 기다리세요. */

        wait(NULL);  /* placeholder - TODO 완성 후 적절한 위치로 이동 */
    }

    return 0;
}
