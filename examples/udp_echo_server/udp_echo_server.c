#include "udp_echo_server.h"
#include "ucos_ii.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

void udp_echo_server_task(void *p_arg)
{
	int sock, len;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	char buf[64];

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7);
	bind(sock, (struct sockaddr *)&addr, addr_len);
	while (1) {
		len = recvfrom(sock, buf, sizeof(buf), 0,
				(struct sockaddr *)&addr, &addr_len);
		printf("%s:%hu\n", inet_ntoa(addr.sin_addr),
				ntohs(addr.sin_port));
		sendto(sock, buf, len, 0,
				(struct sockaddr *)&addr, addr_len);
	}
}
