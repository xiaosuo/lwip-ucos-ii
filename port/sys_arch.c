#include "lwip/sys.h"

#include "ucos_ii.h"

/******************************************************************************
 * Define the size of the mbox
 ******************************************************************************/

#define __MBOX_SIZE 1

#if defined(TCPIP_MBOX_SIZE) && TCPIP_MBOX_SIZE > __MBOX_SIZE
# undef __MBOX_SIZE
# define __MBOX_SIZE TCPIP_MBOX_SIZE
#endif

#if defined(DEFAULT_ACCEPTMBOX_SIZE) && DEFAULT_ACCEPTMBOX_SIZE > __MBOX_SIZE
# undef __MBOX_SIZE
# define __MBOX_SIZE DEFAULT_ACCEPTMBOX_SIZE
#endif

#if defined(DEFAULT_RAW_RECVMBOX_SIZE) && DEFAULT_RAW_RECVMBOX_SIZE > __MBOX_SIZE
# undef __MBOX_SIZE
# define __MBOX_SIZE DEFAULT_RAW_RECVMBOX_SIZE
#endif

#if defined(DEFAULT_UDP_RECVMBOX_SIZE) && DEFAULT_UDP_RECVMBOX_SIZE > __MBOX_SIZE
# undef __MBOX_SIZE
# define __MBOX_SIZE DEFAULT_UDP_RECVMBOX_SIZE
#endif

#if defined(DEFAULT_TCP_RECVMBOX_SIZE) && DEFAULT_TCP_RECVMBOX_SIZE > __MBOX_SIZE
# undef __MBOX_SIZE
# define __MBOX_SIZE DEFAULT_TCP_RECVMBOX_SIZE
#endif

#define __ESC_NULL ((void *)0xffffffffUL)

static struct sys_mbox {
	OS_EVENT	*q;
	OS_EVENT	*sem;
	void		*start[__MBOX_SIZE];
} __mbox[OS_MAX_QS];
static OS_MEM *__mbox_mem;

#if TCPIP_THREAD_STACKSIZE > 0
static OS_STK __tcpip_stk[TCPIP_THREAD_STACKSIZE];
#endif

#if SLIPIF_THREAD_STACKSIZE > 0
static OS_STK __slipif_stk[SLIPIF_THREAD_STACKSIZE];
#endif

#if PPP_THREAD_STACKSIZE > 0
static OS_STK __ppp_stk[PPP_THREAD_STACKSIZE];
#endif

/* sys_init() must be called before anything else. */
void sys_init(void)
{
	INT8U err;

	__mbox_mem = OSMemCreate(&__mbox[0], OS_MAX_QS, sizeof(struct sys_mbox),
		       	&err);
	LWIP_ASSERT("OSMemCreate", err == OS_ERR_NONE);
}

static u32_t ticks_to_ms(u32_t ticks)
{
	return ticks * 1000 / OS_TICKS_PER_SEC;
}

static u32_t ms_to_ticks(u32_t ms)
{
	return ms * OS_TICKS_PER_SEC / 1000;
}

/** Create a new semaphore
 * @param sem pointer to the semaphore to create
 * @param count initial count of the semaphore
 * @return ERR_OK if successful, another err_t otherwise */
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	OS_EVENT *ev;

	ev = OSSemCreate(count);
	if (ev) {
		*sem = ev;
		return ERR_OK;
	}

	return ERR_MEM;
}

/** Delete a semaphore
 * @param sem semaphore to delete */
void sys_sem_free(sys_sem_t *sem)
{
	INT8U err;

	OSSemDel(*sem, OS_DEL_ALWAYS, &err);
	LWIP_ASSERT("OSSemDel", err == OS_ERR_NONE);
}

/** Signals a semaphore
 * @param sem the semaphore to signal */
void sys_sem_signal(sys_sem_t *sem)
{
	INT8U err = OSSemPost(*sem);
	LWIP_ASSERT("OSSemPost", err == OS_ERR_NONE);
}

/** Wait for a semaphore for the specified timeout
 * @param sem the semaphore to wait for
 * @param timeout timeout in milliseconds to wait (0 = wait forever)
 * @return time (in milliseconds) waited for the semaphore
 *         or SYS_ARCH_TIMEOUT on timeout */
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	INT8U err;
	INT32U begin_time;

	if (timeout) {
		timeout = ms_to_ticks(timeout);
		if (!timeout)
			timeout = 1;
		else if (timeout > 65535)
			timeout = 65535;
	}

	begin_time = OSTimeGet();
	OSSemPend(*sem, timeout, &err);
	switch (err) {
	case OS_ERR_NONE:
		return ms_to_ticks(OSTimeGet() - begin_time);
	case OS_ERR_TIMEOUT:
		break;
	default:
		LWIP_ASSERT("OSSemPend", 0);
		break;
	}

	return SYS_ARCH_TIMEOUT;
}

/** Create a new mbox of specified size
 * @param mbox pointer to the mbox to create
 * @param size (miminum) number of messages in this mbox
 * @return ERR_OK if successful, another err_t otherwise */
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	sys_mbox_t m;
	INT8U err;

	LWIP_ASSERT("allocate a empty mbox?", size);
	LWIP_ASSERT("__MBOX_SIZE is too small", size <= __MBOX_SIZE);

	m = OSMemGet(__mbox_mem, &err);
	if (m) {
		m->q = OSQCreate(m->start, __MBOX_SIZE);
		if (m->q) {
			m->sem = OSSemCreate(__MBOX_SIZE);
			if (m->sem) {
				*mbox = m;
				return ERR_OK;
			}
			OSQDel(m->q, OS_DEL_ALWAYS, &err);
			LWIP_ASSERT("OSQDel", err == OS_ERR_NONE);
		}
		err = OSMemPut(__mbox_mem, m);
		LWIP_ASSERT("OSMemPut", err == OS_ERR_NONE);
	}

	return ERR_MEM;
}

/** Delete an mbox
 * @param mbox mbox to delete */
void sys_mbox_free(sys_mbox_t *mbox)
{
	INT8U err;
	sys_mbox_t m = *mbox;

	OSSemDel(m->sem, OS_DEL_ALWAYS, &err);
	LWIP_ASSERT("OSSemDel", err == OS_ERR_NONE);
	OSQDel(m->q, OS_DEL_ALWAYS, &err);
	LWIP_ASSERT("OSQDel", err == OS_ERR_NONE);
	err = OSMemPut(__mbox_mem, m);
	LWIP_ASSERT("OSMemPut", err == OS_ERR_NONE);
}

/** Post a message to an mbox - may not fail
 * -> blocks if full, only used from tasks not from ISR
 * @param mbox mbox to posts the message
 * @param msg message to post (ATTENTION: can be NULL) */
void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
	INT8U err;
	sys_mbox_t m = *mbox;

	if (!msg)
		msg = __ESC_NULL;
	OSSemPend(m->sem, 0, &err);
	LWIP_ASSERT("OSSemPend", err == OS_ERR_NONE);
	err = OSQPost(m->q, msg);
	LWIP_ASSERT("OSQPost", err == OS_ERR_NONE);
}

/** Try to post a message to an mbox - may fail if full or ISR
 * @param mbox mbox to posts the message
 * @param msg message to post (ATTENTION: can be NULL) */
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
	INT8U err;
	sys_mbox_t m = *mbox;

	if (!msg)
		msg = __ESC_NULL;
	if (OSSemAccept(m->sem)) {
		err = OSQPost(m->q, msg);
		LWIP_ASSERT("OSQPost", err == OS_ERR_NONE);
		return ERR_OK;
	} else {
		return ERR_MEM;
	}
}

/** Wait for a new message to arrive in the mbox
 * @param mbox mbox to get a message from
 * @param msg pointer where the message is stored
 * @param timeout maximum time (in milliseconds) to wait for a message (0 = wait forever)
 * @return time (in milliseconds) waited for a message, may be 0 if not waited
           or SYS_ARCH_TIMEOUT on timeout
 *         The returned time has to be accurate to prevent timer jitter! */
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	INT8U err;
	sys_mbox_t m = *mbox;
	INT32U begin_time;

	if (timeout) {
		timeout = ms_to_ticks(timeout);
		if (!timeout)
			timeout = 1;
		else if (timeout > 65535)
			timeout = 65535;
	}
	begin_time = OSTimeGet();
	*msg = OSQPend(m->q, timeout, &err);
	if (err == OS_ERR_NONE) {
		LWIP_ASSERT("OSQPend", *msg);
		err = OSSemPost(m->sem);
		LWIP_ASSERT("OSSemPost", err == OS_ERR_NONE);
		if ((*msg) == __ESC_NULL)
			*msg = NULL;
		return ticks_to_ms(OSTimeGet() - begin_time);
	} else {
		LWIP_ASSERT("OSQPend", err == OS_ERR_TIMEOUT);
		return SYS_ARCH_TIMEOUT;
	}
}

/** Wait for a new message to arrive in the mbox
 * @param mbox mbox to get a message from
 * @param msg pointer where the message is stored
 * @return 0 (milliseconds) if a message has been received
 *         or SYS_MBOX_EMPTY if the mailbox is empty */
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	INT8U err;
	sys_mbox_t m = *mbox;

	*msg = OSQAccept(m->q, &err);
	if (*msg) {
		LWIP_ASSERT("OSQAccept", err == OS_ERR_NONE);
		err = OSSemPost(m->sem);
		LWIP_ASSERT("OSSemPost", err == OS_ERR_NONE);
		if ((*msg) == __ESC_NULL)
			*msg = NULL;
		return 0;
	} else {
		LWIP_ASSERT("OSQAccept", err == OS_ERR_Q_EMPTY);
		return SYS_MBOX_EMPTY;
	}
}

/** The only thread function:
 * Creates a new thread
 * @param name human-readable name for the thread (used for debugging purposes)
 * @param thread thread-function
 * @param arg parameter passed to 'thread'
 * @param stacksize stack size in bytes for the new thread (may be ignored by ports)
 * @param prio priority of the new thread (may be ignored by ports) */
sys_thread_t sys_thread_new(const char *name, void (*thread)(void *arg),
		void *arg, int stacksize, int prio)
{
	INT8U err;
	OS_STK *stk_top;

	LWIP_ASSERT("Non-positive prio", prio > 0);
	LWIP_ASSERT("Prio is too big", prio < OS_PRIO_SELF);
	switch (prio) {
#if TCPIP_THREAD_STACKSIZE > 0
	case TCPIP_THREAD_PRIO:
		stk_top = &__tcpip_stk[TCPIP_THREAD_STACKSIZE - 1];
		break;
#endif
#if SLIPIF_THREAD_STACKSIZE > 0
	case SLIPIF_THREAD_PRIO:
		stk_top = &__slipif_stk[SLIPIF_THREAD_STACKSIZE - 1];
		break;
#endif
#if PPP_THREAD_STACKSIZE > 0
	case PPP_THREAD_PRIO:
		stk_top = &__ppp_stk[PPP_THREAD_STACKSIZE - 1];
		break;
#endif
	default:
		LWIP_ASSERT("Invalid prio", 0);
	}

	/* TODO: OSTaskCreateEx */
	err = OSTaskCreate(thread, arg, stk_top, prio);
	LWIP_ASSERT("OSTaskCreate", err == OS_ERR_NONE);
	OSTaskNameSet(prio, (INT8U *)name, &err);
	LWIP_ASSERT("OSTaskNameSet", err == OS_ERR_NONE);

	return prio;
}

/**
 * Free a thread
 * @param id the thread ID returned by sys_thread_new() */
void sys_thread_free(sys_thread_t id)
{
	INT8U err;

	LWIP_ASSERT("Invalid thread", id != OS_PRIO_SELF);
	err = OSTaskDel(id);
	LWIP_ASSERT("OSTaskDel", err == OS_ERR_NONE);
}

/** Returns the current time in milliseconds,
 * may be the same as sys_jiffies or at least based on it. */
u32_t sys_now(void)
{
	return ticks_to_ms(sys_jiffies());
}
