
#ifndef __LINUX_BLK_UID_THROTTLE_H
#define __LINUX_BLK_UID_THROTTLE_H

#include <linux/uidgid.h>
#include <linux/list.h>
#include <linux/fs.h>

struct blk_uid_rl {
	uid_t uid;
	int ratelimit;
	unsigned long timestamp;
	unsigned long quota;
	struct list_head list;

	/* For debug */
	unsigned long stats_quota;
	unsigned long stats_hz;
	unsigned long last_written;
};

void blk_uid_rl_throttle(struct address_space *mapping, ssize_t written);

#endif /* __LINUX_BLK_UID_THROTTLE_H */
