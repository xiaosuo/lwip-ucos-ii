#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__

#include "ucos_ii.h"

#include <stdlib.h>

/*****************************************************************************
 * sys_mutex
 *****************************************************************************/

#ifndef LWIP_COMPAT_MUTEX
# define LWIP_COMPAT_MUTEX 1
#elif LWIP_COMPAT_MUTEX <= 0
# undef LWIP_COMPAT_MUTEX
# define LWIP_COMPAT_MUTEX 1
#endif

/*****************************************************************************
 * sys_sem
 *****************************************************************************/

#if OS_SEM_EN <= 0
# error "OS_SEM_EN isn't enabled"
#endif

typedef OS_EVENT	*sys_sem_t;

#define SYS_SEM_NULL	NULL

/** Check if a sempahore is valid/allocated: return 1 for valid, 0 for invalid */
#define sys_sem_valid(sem) ((sem) && *(sem))

/** Set a semaphore invalid so that sys_sem_valid returns 0 */
#define sys_sem_set_invalid(sem) \
do { \
	if ((sem)) \
		*(sem) = NULL; \
} while (0)

/*****************************************************************************
 * sys_mbox
 *****************************************************************************/

#if OS_Q_EN <= 0 || OS_MAX_QS <= 0
# error "OS_Q_EN or OS_MAX_QS isn't enabled"
#endif

#if OS_MEM_EN <= 0 || OS_MAX_MEM_PART <= 0
# error "OS_MEM_EN or OS_MAX_MEM_PART isn't enabled"
#endif

struct sys_mbox;

typedef struct sys_mbox	*sys_mbox_t;

#define SYS_MBOX_NULL	NULL

/** Set an mbox invalid so that sys_mbox_valid returns 0 */
#define sys_mbox_valid(mbox) (*(mbox))

/** Check if an mbox is valid/allocated: return 1 for valid, 0 for invalid */
#define sys_mbox_set_invalid(mbox) \
do { \
	if ((mbox)) \
		*(mbox) = NULL; \
} while (0)

/*****************************************************************************
 * sys_thread
 *****************************************************************************/

typedef INT8U		sys_thread_t;

void sys_thread_free(sys_thread_t id);

/*****************************************************************************
 * time
 *****************************************************************************/

/** Ticks/jiffies since power up. */
#define sys_jiffies() OSTimeGet()

#endif /* __ARCH_SYS_ARCH_H__ */
