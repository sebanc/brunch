#ifndef BACKPORT_TSO_H
#define BACKPORT_TSO_H

#include <net/ip.h>

#if LINUX_VERSION_IS_LESS(4,4,0)

#define tso_t LINUX_BACKPORT(tso_t)
struct tso_t {
	int next_frag_idx;
	void *data;
	size_t size;
	u16 ip_id;
	bool ipv6;
	u32 tcp_seq;
};

#define tso_count_descs LINUX_BACKPORT(tso_count_descs)
int tso_count_descs(struct sk_buff *skb);

#define tso_build_hdr LINUX_BACKPORT(tso_build_hdr)
void tso_build_hdr(struct sk_buff *skb, char *hdr, struct tso_t *tso,
		   int size, bool is_last);
#define tso_build_data LINUX_BACKPORT(tso_build_data)
void tso_build_data(struct sk_buff *skb, struct tso_t *tso, int size);
#define tso_start LINUX_BACKPORT(tso_start)
void tso_start(struct sk_buff *skb, struct tso_t *tso);

#else
#include_next <net/tso.h>
#endif

#endif	/* BACKPORT_TSO_H */
