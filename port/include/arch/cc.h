#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include "ucos_ii.h"
#include "sio_cpu.h"

typedef INT8U	u8_t;
typedef INT8S	s8_t;
typedef INT16U	u16_t;
typedef INT16S	s16_t;
typedef INT32U	u32_t;
typedef INT32S	s32_t;

typedef unsigned long mem_ptr_t;

#ifdef __GNUC__ /* GCC */
# define PACK_STRUCT_FIELD(x)	x
# define PACK_STRUCT_STRUCT	__attribute__((__packed__))
# define PACK_STRUCT_BEGIN
# define PACK_STRUCT_END
#elif defined(__CC_ARM) /* Keil C ARM CC */
# define PACK_STRUCT_FIELD(x)	x
# define PACK_STRUCT_STRUCT	
# define PACK_STRUCT_BEGIN	__packed
# define PACK_STRUCT_END
#elif defined(__IAR_SYSTEMS_ICC__) /* IAR */
# define PACK_STRUCT_FIELD(x)	x
# define PACK_STRUCT_STRUCT	
# define PACK_STRUCT_BEGIN
# define PACK_STRUCT_END
# define PACK_STRUCT_USE_INCLUDES
#elif defined(__TASKING__)
# define PACK_STRUCT_FIELD(x)	x
# define PACK_STRUCT_STRUCT	
# define PACK_STRUCT_BEGIN
# define PACK_STRUCT_END
#else
# error "not supported"
#endif

#ifndef LWIP_PLATFORM_DIAG
# define LWIP_PLATFORM_DIAG(x) \
do { \
	printf x; \
} while(0)
#endif

#ifndef LWIP_PLATFORM_ASSERT
# define LWIP_PLATFORM_ASSERT(x) \
do { \
	printf("Assertion \"%s\" failed at line %d in %s\n", \
			x, __LINE__, __FILE__); \
	while (1) \
		; \
} while(0)
#endif

#define X8_F	"02x"
#define U16_F	"hu"
#define S16_F	"hd"
#define X16_F	"hx"
#define U32_F	"u"
#define S32_F	"d"
#define X32_F	"x"
#define SZT_F	"lu"

#if OS_CRITICAL_METHOD == 3
# define SYS_ARCH_DECL_PROTECT(x)	OS_CPU_SR x
# define SYS_ARCH_PROTECT(x)		x = OS_CPU_SR_Save()
# define SYS_ARCH_UNPROTECT(x)		OS_CPU_SR_Restore(x)
#else
# error "not supported"
#endif

#ifndef BYTE_ORDER
# define BYTE_ORDER LITTLE_ENDIAN
#endif

#define LWIP_PROVIDE_ERRNO

#ifndef __sio_fd_t_defined
typedef void *sio_fd_t;
#define __sio_fd_t_defined
#endif

#ifndef sio_rx_ok
u8_t sio_rx_ok(sio_fd_t fd);
#endif

#ifndef sio_rx
u8_t sio_rx(sio_fd_t fd);
#endif

#ifndef sio_tx_ok
u8_t sio_tx_ok(sio_fd_t fd);
#endif

#ifndef sio_tx
void sio_tx(sio_fd_t fd, u8_t c);
#endif

#ifndef sio_enable_tx_irq
void sio_enable_tx_irq(sio_fd_t fd);
#endif

#ifndef sio_disable_tx_irq
void sio_disable_tx_irq(sio_fd_t fd);
#endif

void sio_rx_complete(sio_fd_t fd);
void sio_tx_complete(sio_fd_t fd);

#endif /* __ARCH_CC_H__ */
