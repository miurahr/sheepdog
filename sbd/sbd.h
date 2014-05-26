#ifndef _SBD_H_
#define _SBD_H_

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/gfp.h>

#include "sheepdog_proto.h"

#define DRV_NAME "sbd"
#define DEV_NAME_LEN 32
#define SBD_MINORS_PER_MAJOR 32
#define SECTOR_SIZE 512

struct sheep_vdi {
	struct sd_inode *inode;
	u32 vid;
	char ip[16];
	unsigned int port;
	char name[SD_MAX_VDI_LEN];
};

struct sbd_device {
	struct socket *sock;
	int id;		/* blkdev unique id */
	atomic_t seq_num;

	int major;
	int minor;
	struct gendisk *disk;
	struct request_queue *rq;
	spinlock_t queue_lock;   /* request queue lock */

	struct sheep_vdi vdi;		/* Associated sheep image */

	struct list_head inflight_head;
	wait_queue_head_t inflight_wq;
	struct list_head blocking_head;

	struct list_head list;
	struct task_struct *reaper;
};

struct sheep_aiocb {
	struct request *request;
	u64 offset;
	u64 length;
	int ret;
	u32 nr_requests;
	char *buf;
	int buf_iter;
	void (*aio_done_func)(struct sheep_aiocb *, bool);
};

enum sheep_request_type {
	SHEEP_READ,
	SHEEP_WRITE,
	SHEEP_CREATE,
};

struct sheep_request {
	struct list_head list;
	struct sheep_aiocb *aiocb;
	u64 oid;
	u32 seq_num;
	int type;
	int offset;
	int length;
	char *buf;
};

void socket_shutdown(struct socket *sock);
int sheep_setup_vdi(struct sbd_device *dev);
struct sheep_aiocb *sheep_aiocb_setup(struct request *req);
int sheep_aiocb_submit(struct sheep_aiocb *aiocb);
int sheep_handle_reply(struct sbd_device *dev);

#if defined(CONFIG_DYNAMIC_DEBUG) && defined _DPRINTK_FLAGS_INCL_MODNAME

# define _SBD_FLAGS (_DPRINTK_FLAGS_PRINT | _DPRINTK_FLAGS_INCL_MODNAME \
	| _DPRINTK_FLAGS_INCL_FUNCNAME | _DPRINTK_FLAGS_INCL_LINENO)

# define SBD_DYNAMIC_DEBUG_METADATA(name, fmt)                  \
	static struct _ddebug  __aligned(8)                     \
	 __attribute__((section("__verbose"))) name = {          \
		.modname = KBUILD_MODNAME,                      \
		.function = __func__,                           \
		.filename = __FILE__,                           \
		.format = (fmt),                                \
		.lineno = __LINE__,                             \
		.flags =  _SBD_FLAGS,                           \
	}

# define sbd_debug(fmt, ...)                            \
({                                                      \
	SBD_DYNAMIC_DEBUG_METADATA(descriptor, fmt);    \
	__dynamic_pr_debug(&descriptor, pr_fmt(fmt),    \
			   ##__VA_ARGS__);              \
})

#else

/* If -DDEBUG is not set, pr_debug = no_printk */
# define sbd_debug pr_debug

#endif /* CONFIG_DYNAMIC_DEBUG */

#endif /* _SBD_H_ */