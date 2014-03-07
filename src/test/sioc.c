/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#include "rrutil.h"

const char* sockaddr_name(const struct sockaddr* addr) {
	const struct sockaddr_in* sin = (const struct sockaddr_in*)addr;
	static char str[PATH_MAX];
	/* FIXME: add INET6 support (original author didn't
	 * have ipv6 iface available to test).  */
	test_assert(AF_INET == addr->sa_family);
	return inet_ntop(AF_INET, (void*)&sin->sin_addr, str, sizeof(str));
}

/**
 * Fetch and print the ifconfig for this machine.  Fill in
 * |req.ifr_name| with the first non-loopback interface name found.
 */
static void get_ifconfig(int sockfd, struct ifreq* req) {
	struct ifreq ifaces[100];
	struct ifconf ifconf;
	int ret;
	ssize_t num_ifaces;
	int i;
	int set_req_iface = 0;

	ifconf.ifc_len = sizeof(ifaces);
	ifconf.ifc_req = ifaces;

	ret = ioctl(sockfd, SIOCGIFCONF, &ifconf);
	num_ifaces = ifconf.ifc_len / sizeof(ifaces[0]);
	atomic_printf("SIOCGIFCONF(ret %d): %d ifaces (%d bytes of ifreq)\n",
		      ret, num_ifaces, ifconf.ifc_len);
	test_assert(0 == ret);
	test_assert(0 == (ifconf.ifc_len % sizeof(ifaces[0])));

	for (i = 0; i < num_ifaces; ++i) {
		const struct ifreq* ifc = &ifconf.ifc_req[i];
		atomic_printf("  iface %d: name:%s addr:%s\n", i,
			      ifc->ifr_name, sockaddr_name(&ifc->ifr_addr));
		if (!set_req_iface && strcmp("lo", ifc->ifr_name)) {
			strcpy(req->ifr_name, ifc->ifr_name);
			set_req_iface = 1;
		}
	}
	test_assert(set_req_iface);
}

int main(int argc, char *argv[]) {
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq req;
	char name[PATH_MAX];
	int index;
	struct ethtool_cmd etc;
	int ret;

	get_ifconfig(sockfd, &req);
	strcpy(name, req.ifr_name);

	req.ifr_ifindex = -1;
	strcpy(req.ifr_name, name);
	ret = ioctl(sockfd, SIOCGIFINDEX, &req);
	atomic_printf("SIOCGIFINDEX(ret:%d): %s index is %d\n", ret,
		      req.ifr_name, req.ifr_ifindex);
	test_assert(0 == ret);
	index = req.ifr_ifindex;

	memset(&req.ifr_name, 0x5a, sizeof(req.ifr_name));
	req.ifr_ifindex = index;
	ret = ioctl(sockfd, SIOCGIFNAME, &req);
	atomic_printf("SIOCGIFNAME(ret:%d): index %d(%s) name is %s\n", ret,
		      index, name, req.ifr_name);
	test_assert(0 == ret);
	test_assert(!strcmp(name, req.ifr_name));

	memset(&req.ifr_addr, 0x5a, sizeof(req.ifr_addr));
	ret = ioctl(sockfd, SIOCGIFADDR, &req);
	atomic_printf("SIOCGIFADDR(ret:%d): %s addr is", ret, req.ifr_name);
	atomic_printf(" %s\n", sockaddr_name(&req.ifr_addr));
	test_assert(0 == ret);

	memset(&req.ifr_flags, 0x5a, sizeof(req.ifr_flags));
	ret = ioctl(sockfd, SIOCGIFFLAGS, &req);
	atomic_printf("SIOCGIFFLAGS(ret:%d): %s flags are", ret, req.ifr_name);
	test_assert(0 == ret);
	atomic_printf(" %#x\n", req.ifr_flags);

	memset(&req.ifr_flags, 0x5a, sizeof(req.ifr_mtu));
	ret = ioctl(sockfd, SIOCGIFMTU, &req);
	atomic_printf("SIOCGIFMTU(ret:%d): %s MTU is", ret, req.ifr_name);
	test_assert(0 == ret);
	atomic_printf(" %d\n", req.ifr_mtu);

	memset(&etc, 0, sizeof(etc));
	etc.cmd = ETHTOOL_GSET;
	req.ifr_data = (char*)&etc;
	ret = ioctl(sockfd, SIOCETHTOOL, &req);
	atomic_printf("SIOCETHTOOL(ret:%d): %s ethtool data:\n",
		      ret, req.ifr_name);
	test_assert(0 == ret);
	atomic_printf(
"  speed:%#x duplex:%#x port:%#x physaddr:%#x, maxtxpkt:%u maxrxpkt:%u ...\n",
ethtool_cmd_speed(&etc), etc.duplex, etc.port, etc.phy_address,
etc.maxtxpkt, etc.maxrxpkt);

	/* TODO: read SIOCGIWRATE for wifi interfaces. */

	atomic_puts("EXIT-SUCCESS");
	return 0;
}