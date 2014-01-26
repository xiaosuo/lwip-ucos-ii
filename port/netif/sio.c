#include "ucos_ii.h"
#include "sio_cpu.h"

#include "lwip/sys.h"
#include "lwip/sio.h"

#define __SIO_BUF_SIZE 64

struct __sio_buf {
	INT8U	buf[__SIO_BUF_SIZE];
	INT8U	rd;
	INT8U	wr;
	INT8U	len;
};

struct {
	struct {
		struct __sio_buf	buf;
		OS_EVENT		*sem;
	} rx, tx;
} __sio;

#define __sio_buf_empty(buf) ((buf)->len == 0)
#define __sio_buf_full(buf) ((buf)->len == __SIO_BUF_SIZE)

static void __sio_init_buf(struct __sio_buf *buf)
{
	buf->rd = 0;
	buf->wr = 0;
	buf->len = 0;
}

static INT8U __sio_read_buf(struct __sio_buf *buf)
{
	INT8U c = buf->buf[buf->rd++];

	if (buf->rd == __SIO_BUF_SIZE)
		buf->rd = 0;
	buf->len--;

	return c;
}

static void __sio_write_buf(struct __sio_buf *buf, INT8U c)
{
	buf->buf[buf->wr++] = c;
	if (buf->wr == __SIO_BUF_SIZE)
		buf->wr = 0;
	buf->len++;
}

/**
 * Opens a serial device for communication.
 * 
 * @param devnum device number
 * @return handle to serial device if successful, NULL otherwise
 */
sio_fd_t sio_open(u8_t devnum)
{
	__sio.rx.sem = OSSemCreate(0); /* number of bytes */
	LWIP_ASSERT("OSSemCreate", __sio.rx.sem);
	__sio.tx.sem = OSSemCreate(__SIO_BUF_SIZE + 1); /* number of spaces */
	LWIP_ASSERT("OSSemCreate", __sio.tx.sem);
	__sio_init_buf(&__sio.rx.buf);
	__sio_init_buf(&__sio.tx.buf);

	return &__sio;
}

/**
 * Sends a single character to the serial device.
 * 
 * @param c character to send
 * @param fd serial device handle
 * 
 * @note This function will block until the character can be sent.
 */
void sio_send(u8_t c, sio_fd_t fd)
{
	INT8U err;
	SYS_ARCH_DECL_PROTECT(sr);

	OSSemPend(__sio.tx.sem, 0, &err);
	LWIP_ASSERT("OSSemPend", err == OS_ERR_NONE);
	SYS_ARCH_PROTECT(sr);
	if (__sio_buf_empty(&__sio.tx.buf) && sio_tx_ok(fd)) {
		sio_tx(fd, c);
		sio_enable_tx_irq(fd);
	} else {
		__sio_write_buf(&__sio.tx.buf, c);
	}
	SYS_ARCH_UNPROTECT(sr);
}

/* Called in TX completion ISR */
void sio_tx_complete(sio_fd_t fd)
{
	INT8U err, c;
	SYS_ARCH_DECL_PROTECT(sr);

	SYS_ARCH_PROTECT(sr);
	if (sio_tx_ok(fd)) {
		err = OSSemPost(__sio.tx.sem);
		LWIP_ASSERT("OSSemPost", err == OS_ERR_NONE);
		if (!__sio_buf_empty(&__sio.tx.buf)) {
			c = __sio_read_buf(&__sio.tx.buf);
			sio_tx(fd, c);
		} else {
			sio_disable_tx_irq(fd);
		}
	}
	SYS_ARCH_UNPROTECT(sr);
}

/* Called in RX ISR to push one byte */
void sio_rx_complete(sio_fd_t fd)
{
	INT8U err, c;
	SYS_ARCH_DECL_PROTECT(sr);

	SYS_ARCH_PROTECT(sr);
	if (sio_rx_ok(fd)) {
		c = sio_rx(fd);
		if (!__sio_buf_full(&__sio.rx.buf)) {
			__sio_write_buf(&__sio.rx.buf, c);
			err = OSSemPost(__sio.rx.sem);
			LWIP_ASSERT("OSSemPost", err == OS_ERR_NONE);
		}
	}
	SYS_ARCH_UNPROTECT(sr);
}

/**
 * Receives a single character from the serial device.
 * 
 * @param fd serial device handle
 * 
 * @note This function will block until a character is received.
 */
u8_t sio_recv(sio_fd_t fd)
{
	INT8U err, c = 0;
	SYS_ARCH_DECL_PROTECT(sr);

	OSSemPend(__sio.rx.sem, 0, &err);
	switch (err) {
	case OS_ERR_NONE:
		SYS_ARCH_PROTECT(sr);
		c = __sio_read_buf(&__sio.rx.buf);
		SYS_ARCH_UNPROTECT(sr);
		break;
	case OS_ERR_PEND_ABORT:
		sys_thread_free(PPP_THREAD_PRIO);
	default:
		LWIP_ASSERT("OSQPend", 0);
		break;
	}

	return c;
}

/**
 * Reads from the serial device.
 * 
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received - may be 0 if aborted by sio_read_abort
 * 
 * @note This function will block until data can be received. The blocking
 * can be cancelled by calling sio_read_abort().
 */
u32_t sio_read(sio_fd_t fd, u8_t *data, u32_t len)
{
	u32_t n = 0;

	if (len-- > 0) {
		*data++ = sio_recv(fd);
		++n;
	}

	n += sio_tryread(fd, data, len);

	return n;
}

/**
 * Tries to read from the serial device. Same as sio_read but returns
 * immediately if no data is available and never blocks.
 * 
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received
 */
u32_t sio_tryread(sio_fd_t fd, u8_t *data, u32_t len)
{
	u32_t n = 0;
	INT8U c;
	SYS_ARCH_DECL_PROTECT(sr);

	while (len-- > 0) {
		if (OSSemAccept(__sio.rx.sem) > 0) {
			SYS_ARCH_PROTECT(sr);
			c = __sio_read_buf(&__sio.rx.buf);
			SYS_ARCH_UNPROTECT(sr);
			*data++ = c;
			++n;
		} else {
			break;
		}
	}

	return n;
}

/**
 * Writes to the serial device.
 * 
 * @param fd serial device handle
 * @param data pointer to data to send
 * @param len length (in bytes) of data to send
 * @return number of bytes actually sent
 * 
 * @note This function will block until all data can be sent.
 */
u32_t sio_write(sio_fd_t fd, u8_t *data, u32_t len)
{
	u32_t n = 0;

	while (len-- > 0) {
		sio_send(*data++, fd);
		n++;
	}

	return n;
}

/**
 * Aborts a blocking sio_read() call.
 * 
 * @param fd serial device handle
 */
void sio_read_abort(sio_fd_t fd)
{
	INT8U err;

	OSSemPendAbort(__sio.rx.sem, OS_PEND_OPT_BROADCAST, &err);
}
