#ifndef BLINK_SYSCALL_H_
#define BLINK_SYSCALL_H_
#include "blink/fds.h"
#include "blink/machine.h"
#include "blink/types.h"

#define INTERRUPTIBLE(x)               \
  do {                                 \
    int rc_;                           \
    rc_ = (x);                         \
    if (rc_ == -1 && errno == EINTR) { \
      if (CheckInterrupt(m)) {         \
        break;                         \
      }                                \
    } else {                           \
      break;                           \
    }                                  \
  } while (1)

extern char *g_blink_path;

void OpSyscall(P);

void SysCloseExec(struct System *);
int SysClose(struct System *, i32);
int SysCloseRange(struct System *, u32, u32, u32);
int SysDup(struct Machine *, i32, i32, i32, i32);
int SysOpenat(struct Machine *, i32, i64, i32, i32);
int SysPipe(struct Machine *, i64, i32);
_Noreturn void SysExitGroup(struct Machine *, int);
_Noreturn void SysExit(struct Machine *, int);

void AddStdFd(struct Fds *, int);
int GetAfd(struct Machine *, int, struct Fd **);
int GetFildes(struct Machine *, int);
struct Fd *GetAndLockFd(struct Machine *, int);
bool CheckInterrupt(struct Machine *);

#endif /* BLINK_SYSCALL_H_ */
