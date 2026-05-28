/*
 * user/tracetool.c
 * -----------------
 * 시스템콜 추적기의 사용자 인터페이스.
 *
 * 사용법:
 *   $ tracetool on        # 추적 시작 (이 명령의 자식, 그리고 동일 셸 세션의
 *                         #            자식 명령들이 자동으로 추적됨)
 *   $ tracetool dump      # 통계 JSON 출력
 *   $ tracetool off       # 추적 종료 (통계는 dump로 계속 조회 가능)
 *
 * dump 출력 예시:
 *   {"pid":4,"traced":1,"total":18,"errors":0,
 *    "calls":{"fork":1,"exec":1,"read":3,"write":5,"close":2,"exit":1}}
 *
 * 이 JSON을 LLM 진단 모듈에 stdin으로 흘려보내면 자연어 진단 가능:
 *   $ tracetool dump | llm_diag
 *
 * [xv6 셸의 fork/exec 특성 주의]
 *   xv6 셸은 명령마다 fork+exec를 한다. 따라서 한 셸 세션에서
 *   "tracetool on" 자식이 죽으면 그 효과가 부모 셸로 돌아오지 않는다.
 *   → 패치에서 kfork()가 traced 플래그를 상속하게 했지만, "셸 자신"이
 *      traced=1 이 되어야 자식들이 이어받는다.
 *   → 셸 자체를 추적하고 싶다면 user/sh.c 의 main 시작 부분에
 *      trace_on() 호출 한 줄 추가하면 됨.
 *      (또는 init.c 에서 셸을 띄우기 전에 추적 켜는 방식)
 *
 *   가장 명확한 데모 패턴은 "측정 대상 프로그램 자체"가
 *   시작 시 trace_on(), 종료 직전 trace_stats() 출력을 하는 것.
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/* ============================================================
 * syscall 번호 → 이름 매핑.
 * kernel/syscall.h 의 번호와 정확히 일치해야 함.
 * 이 xv6는 표준과 달리 SYS_sleep 자리에 SYS_pause(13) 가 있음.
 * ============================================================ */
static const char *syscall_name(int nr) {
  switch (nr) {
    case 1:  return "fork";
    case 2:  return "exit";
    case 3:  return "wait";
    case 4:  return "pipe";
    case 5:  return "read";
    case 6:  return "kill";
    case 7:  return "exec";
    case 8:  return "fstat";
    case 9:  return "chdir";
    case 10: return "dup";
    case 11: return "getpid";
    case 12: return "sbrk";
    case 13: return "pause";
    case 14: return "uptime";
    case 15: return "open";
    case 16: return "write";
    case 17: return "mknod";
    case 18: return "unlink";
    case 19: return "link";
    case 20: return "mkdir";
    case 21: return "close";
    case 22: return "trace_on";
    case 23: return "trace_off";
    case 24: return "trace_stats";
    /* K2 fix: integrated syscalls 25..35 */
    case 25: return "thread_create";
    case 26: return "thread_join";
    case 27: return "thread_exit";
    case 28: return "futex_wait";
    case 29: return "futex_wake";
    case 30: return "setpri";
    case 31: return "getstats";
    case 32: return "settrace";
    case 33: return "forkpri";
    case 34: return "ps";
    case 35: return "sysinfo";
    default: return 0;
  }
}

/* ============================================================
 * uint64 → 10진수 문자열 변환.
 * xv6 ulib 의 printf 는 %ld 를 지원하지만, 직접 만들어 두면
 * 향후 출력 포맷 바꾸기 편함. printf 만 쓸 거면 생략해도 됨.
 * ============================================================ */
static int u64_to_str(uint64 v, char *buf) {
  char tmp[24];
  int n = 0;
  if (v == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
  while (v > 0) {
    tmp[n++] = '0' + (v % 10);
    v /= 10;
  }
  for (int i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
  buf[n] = 0;
  return n;
}

/* ============================================================
 * dump_stats: 통계를 받아 JSON으로 stdout 출력.
 * 0인 항목은 출력하지 않음 → LLM 입력 토큰 절약 + 노이즈 감소.
 * ============================================================ */
static void dump_stats(void) {
  struct trace_stats st;
  if (trace_stats(&st) < 0) {
    fprintf(2, "tracetool: trace_stats failed\n");
    exit(1);
  }

  /* 총합/에러 합계 계산 */
  uint64 total = 0, errs = 0;
  for (int i = 0; i < 64; i++) {     /* K2 fix: array now [64] */
    total += st.syscall_count[i];
    errs  += st.syscall_errors[i];
  }

  char num[24];

  printf("{\"pid\":%d,\"traced\":%d,", getpid(), st.traced);

  u64_to_str(total, num);
  printf("\"total\":%s,", num);
  u64_to_str(errs, num);
  printf("\"errors\":%s,", num);

  /* calls 객체: 0이 아닌 항목만 */
  printf("\"calls\":{");
  int first = 1;
  for (int i = 0; i < 64; i++) {     /* K2 fix: array now [64] */
    if (st.syscall_count[i] == 0) continue;
    const char *name = syscall_name(i);
    u64_to_str(st.syscall_count[i], num);
    if (name)
      printf("%s\"%s\":%s", first ? "" : ",", name, num);
    else
      printf("%s\"sys_%d\":%s", first ? "" : ",", i, num);
    first = 0;
  }
  printf("}");

  /* err_calls 객체: 에러가 있을 때만 */
  if (errs > 0) {
    printf(",\"err_calls\":{");
    first = 1;
    for (int i = 0; i < 64; i++) {     /* K2 fix: array now [64] */
      if (st.syscall_errors[i] == 0) continue;
      const char *name = syscall_name(i);
      u64_to_str(st.syscall_errors[i], num);
      printf("%s\"%s\":%s",
             first ? "" : ",",
             name ? name : "sys_?",
             num);
      first = 0;
    }
    printf("}");
  }

  printf("}\n");
}

static void usage(void) {
  fprintf(2,
    "usage:\n"
    "  tracetool on     # start tracing this process (and its children)\n"
    "  tracetool off    # stop tracing\n"
    "  tracetool dump   # print accumulated stats as JSON\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc < 2) usage();

  if (strcmp(argv[1], "on") == 0) {
    trace_on();
    printf("tracing enabled\n");
    exit(0);
  }
  if (strcmp(argv[1], "off") == 0) {
    trace_off();
    printf("tracing disabled\n");
    exit(0);
  }
  if (strcmp(argv[1], "dump") == 0) {
    dump_stats();
    exit(0);
  }

  usage();
  return 0;
}
