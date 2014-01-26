#include "ucos_ii.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/dns.h"

struct stun_header {
	uint16_t	type;
	uint16_t	length;
	uint32_t	transaction_id[4];
};

struct stun_attr_header {
	uint16_t	type;
	uint16_t	length;
	union {
		uint32_t	v32;
		struct {
			uint8_t		unused;
			uint8_t		family;
			uint16_t	port;
			struct in_addr	addr;
		} addr;
	}		value;
};

void stun_task(void *p_arg)
{
	struct hostent *ent;
	struct sockaddr_in addr;
	int sock;
	uint32_t buf[50];
	struct stun_header *stun_hdr;
	struct stun_attr_header *attr_hdr;
	int timeo = 3000;
	ip_addr_t ip;

	ip.addr = inet_addr("8.8.8.8");
	dns_setserver(0, &ip);

	srand(OSTimeGet());
	while (1) {
		OSTimeDly(10 * OS_TICKS_PER_SEC);
		printf("resolving\r\n");
		ent = gethostbyname("stun.stunprotocol.org");

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		if (ent) {
			printf("resolved\r\n");
			memcpy(&addr.sin_addr.s_addr, ent->h_addr, 4);
		} else {
			printf("failed to resolve\r\n");
			addr.sin_addr.s_addr = inet_addr("107.23.150.92");
		}
		addr.sin_port = htons(3478);

		sock = socket(PF_INET, SOCK_DGRAM, 0);
		LWIP_ASSERT("socket", sock >= 0);
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeo,
				sizeof(timeo));
		connect(sock, (struct sockaddr *)&addr, sizeof(addr));

		stun_hdr = (void *)buf;
		stun_hdr->type = htons(1);
		stun_hdr->length = htons(0);
		stun_hdr->transaction_id[0] = rand();

		printf("sending\r\n");
		send(sock, buf, 20, 0);
		printf("sent\r\n");
		if (recv(sock, buf, 100, 0) < 0) {
			close(sock);
			continue;
		}
		printf("received\r\n");

		LWIP_ASSERT("recv", ntohs(stun_hdr->type) == 0x101);
		for (attr_hdr = (void *)(stun_hdr + 1); ((uint8_t *)attr_hdr) < ((uint8_t *)(stun_hdr + 1) + ntohs(stun_hdr->length)); attr_hdr = (struct stun_attr_header *)(((uint8_t *)attr_hdr) + 4 + ntohs(attr_hdr->length))) {
			if (ntohs(attr_hdr->type) == 1) {
				LWIP_ASSERT("type", ntohs(attr_hdr->length) == 8);
				LWIP_ASSERT("family", attr_hdr->value.addr.family == 1);
				printf("%s:%hu\n", inet_ntoa(attr_hdr->value.addr.addr),
						ntohs(attr_hdr->value.addr.port));
			}
		}

		close(sock);
	}
}
