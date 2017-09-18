#include "tracepath.h"
#include <stdlib.h>
#include <vector>
#include <string>
#include <android/log.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <linux/errqueue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>


#define HOST_COLUMN_SIZE	52

void printIcmpHdr(char* title, icmp icmp_hdr) {
    LOG("%s: type=0x%x, id=0x%x, sequence = 0x%x ",
        title, icmp_hdr.icmp_type, icmp_hdr.icmp_id, icmp_hdr.icmp_seq);
}

void print_tracepath_hop_vec(tracepath_hop_vec array) {
    for (tracepath_hop_vec::iterator it = array.begin(); it != array.end(); ++it) {
        LOG("TTL: %d --> %s", it->ttl, it->receive_addr.c_str());
    }
}

Tracepath::Tracepath() {
    return;
}

Tracepath::~Tracepath() {
    LOG_INFO("Renderer instance destroyed");
    return;
}

std::vector<tracepath_hop> Tracepath::runWithDestinationAddress(struct in_addr *dst)
{
    tracepath_hop_vec result; // TODO <-- not done with result yet.

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);

    addr.sin_family = AF_INET;
    addr.sin_addr = *dst;
    char* snd_addy = inet_ntoa(addr.sin_addr);
    std::string snd_addy_str = std::string(snd_addy);

    LOG("----------------------------------------------");
    LOG(" Starting trace to %s", snd_addy);

    int sock = setupSocket(dst);
    if (sock < 0) {
        LOG("Failed to build send socket");
        return result;
    }

    int maxHops = 255;
    int maxProbes = 3;

    for (int ttl = 1; ttl < maxHops; ttl++) {

        // Set TTL on socket
        if (setsockopt(sock, SOL_IP, IP_TTL, &ttl, sizeof(ttl))) {
            LOG("Failed to set IP_TTL");
            return result;
        }

        struct tracepath_hop probe;
        probe.ttl = ttl;

        // Send up to 3 probes
        for (int probeCount = 0; probeCount < maxProbes; probeCount++) {

            if (sendProbe(sock, addr, ttl, probeCount, probe)) {
                if (probe.success) {
                    probeCount = maxProbes; // Break out of inner loop

                    LOG("Pushing probe");
                    result.push_back(probe);

                    if (probe.receive_addr.compare(snd_addy_str) == 0) {
                        // We have reached our destination
                        // Break out of outer loop
                        ttl = maxHops;
                        break;
                    }
                }
            }
        }
    }

    // LEFT OFF, weird pass by reference issues happening with probe hop data.
    print_tracepath_hop_vec(result);

    LOG("----------------------------------------------");
    LOG(" Trace complete");
    LOG("----------------------------------------------");
}

int Tracepath::setupSocket(struct in_addr *dst) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0) {
        perror("socket");
        return sock;
    }

    // TODO if this is not supported, we cannot run tracepath.
    // IP_MTU_DISCOVER: Set or receive the Path MTU Discovery setting for a socket. When enabled, Linux will perform Path MTU Discovery as defined in RFC 1191 on SOCK_STREAM sockets.
    // For non-SOCK_STREAM sockets, IP_PMTUDISC_DO forces the don't-fragment flag to be set on all outgoing packets. It is the user's responsibility to packetize
    // e data in MTU-sized chunks and to do the retransmits if necessary. The kernel will reject (with EMSGSIZE) datagrams that are bigger than the known path MTU.
    // IP_PMTUDISC_WANT will fragment a datagram if needed according to the path MTU, or will set the don't-fragment flag otherwise.
    int on = IP_PMTUDISC_PROBE;
    if (setsockopt(sock, SOL_IP, IP_MTU_DISCOVER, &on, sizeof(on)) && (on = IP_PMTUDISC_DO, setsockopt(sock, SOL_IP, IP_MTU_DISCOVER, &on, sizeof(on)))) {
        LOG("IP_MTU_DISCOVER");
        //exit(1);
    }
    on = 1;

    // SOL_IP: (set/configure various IP packet options, IP layer behaviors, [as here] netfilter module options)
    // IP_RECVERR: Enable extended reliable error message passing. When enabled on a datagram socket,
    // all generated errors will be queued in a per-socket error queue. When the user receives an error from a socket operation,
    // the errors can be received by calling recvmsg with the MSG_ERRQUEUE flag set.
    if (setsockopt(sock, SOL_IP, IP_RECVERR, &on, sizeof(on))) {
        LOG("IP_RECVERR");
        // TODO if this is not supported, we cannot run tracepath.
    }

    // IP_RECVTTL: When this flag is set, pass a IP_TTL control message with the time to live field of the received
    // packet as a byte. Not supported for SOCK_STREAM sockets.
    // TODO, not sure if required for tracepath
    if (setsockopt(sock, SOL_IP, IP_RECVTTL, &on, sizeof(on))) {
        LOG("IP_RECVTTL");
    }

    return sock;
}

int Tracepath::waitForReply(int sock) {
    // Wait for a reply with a timeout
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    return select(sock+1, &fds, NULL, NULL, &tv);
}

struct probehdr
{
    __u32 ttl;
    struct timeval tv;
};

// TODO define this better.
// returns the size of the error received.
bool Tracepath::receiveError(int sock, int ttl, tracepath_hop &probe) {
    struct msghdr msg;
    struct probehdr rcvbuf;
    struct iovec  iov;
    struct sockaddr_in addr;
    char cbuf[512];

    // The recvmsg() call uses a msghdr structure to minimize the number of directly supplied arguments.
    memset(&rcvbuf, -1, sizeof(rcvbuf));
    iov.iov_base = &rcvbuf;
    iov.iov_len = sizeof(rcvbuf);
    msg.msg_name = (__u8*)&addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    // recvmsg: Returns the length of the message on successful completion. If a message is too
    // long to fit in the supplied buffer, excess bytes may be discarded depending on the type of
    // socket the message is received from.

    int res = recvmsg(sock, &msg, MSG_ERRQUEUE);
    if (res < 0) {
        // EAGAIN is often raised when performing non-blocking I/O.
        // It means "there is no data available right now, try again later".
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // If there is no error available, then we may have a valid response coming back.
            LOG("errno == EAGAIN");
            probe.error = EAGAIN;
            return false;
        } else {
            // Else, attempt to read MSG_ERRQUEUE again
            return receiveError(sock, ttl, probe);
        }
    }

//    if (res == sizeof(rcvbuf)) {
//        if (rcvbuf.ttl == 0 || rcvbuf.tv.tv_sec == 0) {
//            //broken_router = 1;
//        } else {
//            LOG("ttl %d", rcvbuf.ttl);
//            LOG("tv %d", rcvbuf.tv);
//        }
//    }

    struct cmsghdr *cmsg;
    struct sock_extended_err *e;
    int rethops;
    int sndhops;

    e = NULL;

    // Parse message into sock_extended_err object.
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_IP) {
            if (cmsg->cmsg_type == IP_RECVERR) {
                e = (struct sock_extended_err *) CMSG_DATA(cmsg);
            } else if (cmsg->cmsg_type == IP_TTL) {
                memcpy(&rethops, CMSG_DATA(cmsg), sizeof(rethops));
            } else {
                LOG("Could not parse sock_extended_err; Invalid cmsg_type:%d\n ", cmsg->cmsg_type);
            }
        }
    }

    if (e == NULL) {
        printf("no info\n");
        return 0;
    }

    if (e->ee_origin == SO_EE_ORIGIN_LOCAL) {
        LOG("%2d?: %*s ", ttl, -(HOST_COLUMN_SIZE - 1), "[LOCALHOST]");
    } else if (e->ee_origin == SO_EE_ORIGIN_ICMP) {
        char abuf[128];
        struct sockaddr_in *sin = (struct sockaddr_in*)(e+1);
        //struct hostent *h = NULL;
        //char *idn = NULL;
        inet_ntop(AF_INET, &sin->sin_addr, abuf, sizeof(abuf));

        // We have pulled out the error for this TTL level, count the probe
        // as a success.
        probe.receive_addr = std::string(abuf);
        probe.success = true;
        probe.error = e->ee_errno;

        LOG("Receive from %s", abuf);

//        if (sndhops>0)
//            LOG("%2d:  ", sndhops);
//        else
//            LOG("%2d?: ", ttl);
    }

    rethops = -1;
    sndhops = -1;

    switch (e->ee_errno) {
        case EHOSTUNREACH:
            LOG("EHOSTUNREACH");
//            if (e->ee_origin == SO_EE_ORIGIN_ICMP &&
//                e->ee_type == 11 &&
//                e->ee_code == 0) {
//                if (rethops>=0) {
//                    if (rethops<=64)
//                        rethops = 65-rethops;
//                    else if (rethops<=128)
//                        rethops = 129-rethops;
//                    else
//                        rethops = 256-rethops;
//                    if (sndhops>=0 && rethops != sndhops)
//                        LOG("asymm %2d ", rethops);
//                    else if (sndhops<0 && rethops != ttl)
//                        LOG("asymm %2d ", rethops);
//                }
//            }
            break;
        case ENETUNREACH:
            LOG("ENETUNREACH");
            break;
        case ETIMEDOUT:
            LOG("ETIMEDOUT");
            // If timed out, then attempt to receive error again.
            return receiveError(sock, ttl, probe);
            break;
        case EMSGSIZE:
            LOG("EMSGSIZE");
            LOG("pmtu %d\n", e->ee_info);
            //mtu = e->ee_info;
            //progress = mtu;
            break;
        case ECONNREFUSED:
            LOG("ECONNREFUSED");
            LOG("reached\n");
            //hops_to = sndhops<0 ? ttl : sndhops;
            //hops_from = rethops;
            break;
        case EPROTO:
            LOG("EPROTO");
            LOG("!P\n");
            break;
        case EACCES:
            LOG("EACCES");
            break;
        default:
            errno = e->ee_errno;
            LOG("NET ERROR");
            break;
    }

    return true;
}

bool Tracepath::receiveData(int sock, tracepath_hop &probe) {
    struct sockaddr_in rcv_addr;
    socklen_t slen = sizeof rcv_addr;

    unsigned char data[2048];
    struct icmp rcv_hdr;

    int rc = recvfrom(sock, data, sizeof data, 0, (struct sockaddr*)&rcv_addr, &slen);
    char* rcv_addy = inet_ntoa(rcv_addr.sin_addr);
    LOG("Receive length, %d bytes from %s", slen, rcv_addy);

    if (rc <= 0) {
        LOG("Failed to recvfrom");
        return false;
    } else if (rc < sizeof rcv_hdr) {
        LOG("Error, got short ICMP packet, %d bytes", rc);
        return false;
    }

    probe.receive_addr = std::string(rcv_addy);
    probe.success = true;

    memcpy(&rcv_hdr, data, sizeof rcv_hdr);
    if (rcv_hdr.icmp_type == ICMP_ECHOREPLY) {
        printIcmpHdr("Received", rcv_hdr);
        return true;
    } else {
        LOG("Got ICMP packet with type 0x%x ?!?", rcv_hdr.icmp_type);
    }

    return false;
}

bool Tracepath::sendProbe(int sock, sockaddr_in addr, int ttl, int attempt, tracepath_hop &probe) {

    struct icmp icmp_hdr;
    struct probehdr probe_hdr;
    struct timeval tv;
    int sequence = 0;

    LOG("-------------------------------------------");
    memset(&icmp_hdr, 0, sizeof icmp_hdr);
    icmp_hdr.icmp_type = ICMP_ECHO;
    int max_attempts = 10;
    int i = 0;
    bool error_msg_received = false;

    for (i=0; i < max_attempts; i++) {

        icmp_hdr.icmp_seq = attempt;
        unsigned char data[2048];
        memcpy(data, &icmp_hdr, sizeof icmp_hdr);
        memcpy(data + sizeof icmp_hdr, "hello", 5); //icmp payload

        LOG("TTL: %d", ttl);
        printIcmpHdr("Sending", icmp_hdr);

        // sendto: On success, return the number of characters sent. On error, -1 is returned, and errno is set appropriately.
        int rc = sendto(sock, data, sizeof icmp_hdr + 5, 0, (struct sockaddr*)&addr, sizeof addr);
        if (rc <= 0) {
            LOG("Failed to send ICMP packet");
            return false;
        }

        bool foundError = receiveError(sock, ttl, probe);
        if (!foundError) {
            LOG("No Error message received, attempting to read incoming data");
            // If there is no error available, then we may have a valid response coming back.
            // TODO Do We want to send out another packet to make sure this is the case?
            continue;
        } else {
            // We have parsed out and handled an error message.
            // This means that we will not be getting data on the socket, so there is no
            // need to continue.
            LOG("Error message received, moving to next TTL");
            error_msg_received = true;
            break;
        }
    }

    if (error_msg_received) {
        // If we received an error we should have logged the data in probe.
        // Count as a successful probe.
        return true;
    } else {
        // No error found, We may have data waiting for us on the socket.
        int reply = waitForReply(sock);
        if (reply == 0) {
            LOG("waitForReply, no reply");
            return false;
        } else if (reply < 0) {
            LOG("Failed waitForReply");
            return false;
        } else {
            return receiveData(sock, probe);
        }
    }
}

