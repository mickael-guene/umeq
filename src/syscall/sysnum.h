#ifndef SYSNUM_H
#define SYSNUM_H

#define SYSNUM(item) PR_ ## item,
typedef enum {
    PR_void = 0,
    #include "sysnums.list"
    PR_NB_SYSNUM
} Sysnum;
#undef SYSNUM

#endif /* SYSNUM_H */