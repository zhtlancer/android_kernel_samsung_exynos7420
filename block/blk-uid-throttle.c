
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/genhd.h>

#include <linux/blk-uid-throttle.h>

#define BLK_UID_DEBUG 0

static uint8_t input_buf[PAGE_SIZE];

struct blk_uid_rl **blk_uid_rl_slots;
LIST_HEAD(blk_uid_rl_list);
DEFINE_SPINLOCK(blk_uid_rl_list_lock);

static inline long throttle_sleep(long timeout)
{
	__set_current_state(TASK_KILLABLE);
	return io_schedule_timeout(timeout);
}

void blk_uid_rl_throttle(struct address_space *mapping, ssize_t written)
{
	struct blk_uid_rl *rl;
	/* debug */
#if BLK_UID_DEBUG
	unsigned long iterations = 0, cmpxchg_iter = 0, stats_alloc = 0;
	static unsigned long func_iter = 0;
	func_iter += 1;
#endif

	if (unlikely(blk_uid_rl_slots == NULL))
		return;

	rl = blk_uid_rl_slots[current->disk_stats_index];

	if (rl == NULL || rl->ratelimit < 0)
		return;

	do {
		unsigned long old_quota, new_quota, allocate;
		unsigned long timestamp = jiffies;
		unsigned long pause;

#if BLK_UID_DEBUG
		iterations += 1;
#endif

		if (rl == NULL || rl->ratelimit < 0)
			return;

		if (unlikely(timestamp < rl->timestamp) ||
				timestamp - rl->timestamp >= HZ) {
			rl->timestamp = timestamp;
			rl->quota = rl->ratelimit;
		}

		pause = HZ - (timestamp - rl->timestamp);

#if BLK_UID_DEBUG
		if (iterations > 1) {
			printk(KERN_WARNING "%s:%d %d %s %p throttled for too long: "
					"func_iter %lu iterations %lu cmpxchg_iter %lu stats_alloc %lu written %d pause %lu\n",
					__func__, __LINE__,
					current->pid, current->comm, &rl,
					func_iter, iterations, cmpxchg_iter, stats_alloc, written, pause);
		}
#endif

		if (rl->quota == 0) {
			throttle_sleep(pause);
			continue;
		}

		old_quota = rl->quota;
		allocate = min(written, (ssize_t)old_quota);
		new_quota = old_quota - allocate;
		if (cmpxchg(&rl->quota, old_quota, new_quota) != old_quota) {
			/* Racing with someone? */
			throttle_sleep(pause / 2);
#if BLK_UID_DEBUG
			printk(KERN_WARNING "%s:%d rl->quota %lu old_quota %lu new_quota %lu\n",
					__func__, __LINE__, rl->quota, old_quota, new_quota);
			cmpxchg_iter += 1;
#endif
			continue;
		}
		written -= allocate;
		rl->stats_quota += allocate;
		rl->last_written = written;

#if BLK_UID_DEBUG
		stats_alloc += allocate;
#endif
		if (written == 0)
			return;
		if (new_quota == 0) {
			rl->stats_hz = pause;
			throttle_sleep(pause);
		}
	} while (1);
}

static int ratelimit_uid_show(struct seq_file *seqf, void *v)
{
	struct blk_uid_rl *ptr;

	list_for_each_entry(ptr, &blk_uid_rl_list, list) {
		seq_printf(seqf, "%d %d ts %lu qa %lu stats_qa %lu slp hz %lu / HZ %d last wr %lu\n",
				ptr->uid, ptr->ratelimit,
				ptr->timestamp, ptr->quota, ptr->stats_quota,
				ptr->stats_hz, HZ,
				ptr->last_written);
	}
	return 0;
}

#if 0
static const struct seq_operations ratelimit_uid_op = {
	.start	= ratelimit_uid_start,
	.next	= ratelimit_uid_next,
	.stop	= ratelimit_uid_stop,
	.show	= ratelimit_uid_show
};
#endif

static int ratelimit_uid_open(struct inode *inode, struct file *file)
{
	return single_open(file, ratelimit_uid_show, NULL);
}

static ssize_t ratelimit_uid_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offp)
{
	uid_t uid;
	int rval, rate, rl_slot_index;
	rval = copy_from_user(input_buf, buf, count);
	input_buf[count] = '\0';

	rval = sscanf(input_buf, "%d %d", &uid, &rate);

	if (rval < 2) {
		printk(KERN_ERR "%s:%d invalid input '%s'\n",
				__func__, __LINE__, input_buf);
		return -EINVAL;
	}

	printk(KERN_WARNING "%s:%d uid %d ratelimit %d\n", __func__, __LINE__, (int)uid, rate);

	if (uid < 0) {
		struct blk_uid_rl *ptr;
		spin_lock(&blk_uid_rl_list_lock);
		list_for_each_entry(ptr, &blk_uid_rl_list, list) {
			ptr->ratelimit = -1;
			ptr->stats_quota = 0;
			ptr->timestamp = 0;
			ptr->stats_hz = 0;
			ptr->last_written = 0;
		}
		spin_unlock(&blk_uid_rl_list_lock);
		return count;
	}

	spin_lock(&disk_stats_uid_slots_lock);
	rl_slot_index = alloc_stats_index(uid);
	spin_unlock(&disk_stats_uid_slots_lock);

	spin_lock(&blk_uid_rl_list_lock);
	if (blk_uid_rl_slots[rl_slot_index] == NULL) {
		struct blk_uid_rl *ptr;
		ptr = kzalloc(sizeof(struct blk_uid_rl), GFP_KERNEL);
		blk_uid_rl_slots[rl_slot_index] = ptr;
		ptr->uid = uid;
		ptr->ratelimit = rate;
		list_add(&ptr->list, &blk_uid_rl_list);
	} else {
		struct blk_uid_rl *ptr =
			blk_uid_rl_slots[rl_slot_index];
		WARN_ON(ptr->uid != uid);
		ptr->ratelimit = rate;
		ptr->stats_quota = 0;
	}
	spin_unlock(&blk_uid_rl_list_lock);

	return count;
}

static const struct file_operations proc_ratelimit_uid_operations = {
	.open		= ratelimit_uid_open,
	.read		= seq_read,
	.write		= ratelimit_uid_write,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init proc_ratelimit_uid_init(void)
{
	blk_uid_rl_slots = kzalloc(sizeof(struct blk_uid_rl *) * MAX_STATS_ENTRIES, GFP_KERNEL);
	if (blk_uid_rl_slots == NULL) {
		printk(KERN_ERR "Failed to allocate memory for blk_uid_rl_slots\n");
		return -ENOMEM;
	}
	proc_create("ratelimit_uid", 0, NULL, &proc_ratelimit_uid_operations);
	return 0;
}
module_init(proc_ratelimit_uid_init);
