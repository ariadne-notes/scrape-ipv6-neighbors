#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <net/if.h>

struct {
    struct nlmsghdr nlh;
    struct ndmsg ndm;
} req;

int main() {
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK };
    bind(fd, (struct sockaddr *)&sa, sizeof(sa));

    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
    req.nlh.nlmsg_type = RTM_GETNEIGH;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nlh.nlmsg_seq = 1;
    req.ndm.ndm_family = AF_INET6;

    send(fd, &req, req.nlh.nlmsg_len, 0);

    char buf[65536];
    int done = 0;
    while (!done) {
        int len = recv(fd, buf, sizeof(buf), 0);
        if (len <= 0) break;

        struct nlmsghdr *nh;
        for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
            if (nh->nlmsg_type == NLMSG_DONE) { done = 1; break; }
            if (nh->nlmsg_type == NLMSG_ERROR) { done = 1; break; }

            struct ndmsg *ndm = NLMSG_DATA(nh);
            struct rtattr *rta = (struct rtattr *)((char *)ndm + NLMSG_ALIGN(sizeof(*ndm)));
            int rtalen = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*ndm));

            char addr[INET6_ADDRSTRLEN] = "?";
            char mac[32] = "?";
            char ifname[IF_NAMESIZE];

            for (; RTA_OK(rta, rtalen); rta = RTA_NEXT(rta, rtalen)) {
                if (rta->rta_type == NDA_DST) {
                    inet_ntop(AF_INET6, RTA_DATA(rta), addr, sizeof(addr));
                }
                if (rta->rta_type == NDA_LLADDR && RTA_PAYLOAD(rta) == 6) {
                    unsigned char *m = RTA_DATA(rta);
                    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                             m[0], m[1], m[2], m[3], m[4], m[5]);
                }
            }

            const char *state = "?";
            if (ndm->ndm_state & NUD_REACHABLE) state = "REACHABLE";
            else if (ndm->ndm_state & NUD_STALE) state = "STALE";
            else if (ndm->ndm_state & NUD_DELAY) state = "DELAY";
            else if (ndm->ndm_state & NUD_PROBE) state = "PROBE";
            else if (ndm->ndm_state & NUD_FAILED) state = "FAILED";
            else if (ndm->ndm_state & NUD_NOARP) state = "NOARP";
            else if (ndm->ndm_state & NUD_PERMANENT) state = "PERMANENT";
            else if (ndm->ndm_state & NUD_INCOMPLETE) state = "INCOMPLETE";

            if_indextoname(ndm->ndm_ifindex, ifname);

            printf("%-45s dev %-8s lladdr %-20s %s\n", addr, ifname, mac, state);
        }
    }

    close(fd);
    return 0;
}
