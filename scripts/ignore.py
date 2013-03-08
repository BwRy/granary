
# TODO: currently have linker or platform errors for these.
IGNORE = set([
  "add_profil",
  
  "profil",
  "unwhiteout",
  "zopen",
  "_IO_cookie_init",
  "matherr",
  "setkey",
  "zopen",

  # apple-only?
  "pthread_rwlock_downgrade_np",
  "pthread_rwlock_upgrade_np",
  "pthread_rwlock_tryupgrade_np",
  "pthread_rwlock_held_np",
  "pthread_rwlock_rdheld_np",
  "pthread_rwlock_wrheld_np",
  "pthread_getname_np",
  "pthread_setname_np",
  "pthread_rwlock_longrdlock_np",
  "pthread_rwlock_yieldwrlock_np",

  # 'dangerous' Linux symbols
  "sigreturn",
  "tmpnam",
  "tmpnam_r",
  "tempnam",
  "gets", # hrmm
  "mktemp",
  "getpw",

  # non-portable?
  "pthread_mutexattr_setrobust",
  "pthread_mutexattr_getrobust",

  # things that are wacky to wrap
  "alloca",

  "setjmp",
  "_setjmp",
  "__setjmp",

  "sigsetjmp",
  "_sigsetjmp",
  "__sigsetjmp",

  "longjmp",
  "_longjmp",
  "__longjmp",

  "siglongjmp",
  "_siglongjmp",
  "__siglongjmp",

  # not necessarily implemented
  "setlogin",
  "lchmod",
  "revoke",
  "getumask",

  # TODO: make wrappers for these
  "pthread_create",
  "pthread_once",

  # these don't need to be wrapped (note: these
  # aren't all necessarily real functions!)
  "exec",
  "execv",
  "execvp",
  "execvpe",
  "execl",
  "execle",
  "execlp",
  "execlpe",
])
