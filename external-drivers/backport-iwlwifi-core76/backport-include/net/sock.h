#ifndef __BACKPORT_NET_SOCK_H
#define __BACKPORT_NET_SOCK_H
#include_next <net/sock.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(4,5,0)
#define sk_set_bit LINUX_BACKPORT(sk_set_bit)
static inline void sk_set_bit(int nr, struct sock *sk)
{
	set_bit(nr, &sk->sk_socket->flags);
}
#endif /* < 4.5 */

#if LINUX_VERSION_IS_LESS(4,5,0)
#define sk_clear_bit LINUX_BACKPORT(sk_clear_bit)
static inline void sk_clear_bit(int nr, struct sock *sk)
{
	clear_bit(nr, &sk->sk_socket->flags);
}
#endif /* < 4.5 */

#if LINUX_VERSION_IS_LESS(4,16,0)
#define sk_pacing_shift_update LINUX_BACKPORT(sk_pacing_shift_update)
static inline void sk_pacing_shift_update(struct sock *sk, int val)
{
#if LINUX_VERSION_IS_GEQ(4,15,0)
	if (!sk || !sk_fullsock(sk) || sk->sk_pacing_shift == val)
		return;
	sk->sk_pacing_shift = val;
#endif /* >= 4.15 */
}
#endif /* < 4.16 */

#if LINUX_VERSION_IS_LESS(5,14,0)
static inline void backport_sk_error_report(struct sock *sk)
{
	sk->sk_error_report(sk);
}
#define sk_error_report(sk) LINUX_BACKPORT(sk_error_report(sk))
#endif /* <= 5.14 */

#endif /* __BACKPORT_NET_SOCK_H */
