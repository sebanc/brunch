#ifndef __BACKPORT_RCULIST_H
#define __BACKPORT_RCULIST_H
#include_next <linux/rculist.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(5,4,0)
/**
 * list_for_each_entry_rcu	-	iterate over rcu list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 * @cond...:	optional lockdep expression if called from non-RCU protection.
 *
 * This list-traversal primitive may safely run concurrently with
 * the _rcu list-mutation primitives such as list_add_rcu()
 * as long as the traversal is guarded by rcu_read_lock().
 */
#undef list_for_each_entry_rcu
#define list_for_each_entry_rcu(pos, head, member, cond...)		\
	for (pos = list_entry_rcu((head)->next, typeof(*pos), member); \
		&pos->member != (head); \
		pos = list_entry_rcu(pos->member.next, typeof(*pos), member))
#endif /* < 5.4 */

#endif /* __BACKPORT_RCULIST_H */
