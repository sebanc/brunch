// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _OSDEP_SERVICE_C_

#include <drv_types.h>

#define RT_TAG	'1178'

/*
* Translate the OS dependent @param error_code to OS independent RTW_STATUS_CODE
* @return: one of RTW_STATUS_CODE
*/
inline int RTW_STATUS_CODE(int error_code)
{
	if (error_code >= 0)
		return _SUCCESS;

	switch (error_code) {
	/* case -ETIMEDOUT: */
	/*	return RTW_STATUS_TIMEDOUT; */
	default:
		return _FAIL;
	}
}

u32 rtw_atoi(u8 *s)
{

	int num = 0, flag = 0;
	int i;
	for (i = 0; i <= strlen(s); i++) {
		if (s[i] >= '0' && s[i] <= '9')
			num = num * 10 + s[i] - '0';
		else if (s[0] == '-' && i == 0)
			flag = 1;
		else
			break;
	}

	if (flag == 1)
		num = num * -1;

	return num;

}

void *_rtw_malloc(u32 sz)
{
	void *pbuf = NULL;

#ifdef RTK_DMP_PLATFORM
	if (sz > 0x4000)
		pbuf = dvr_malloc(sz);
	else
#endif
		pbuf = kmalloc(sz, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	return pbuf;
}

void *_rtw_zmalloc(u32 sz)
{
	void *pbuf = _rtw_malloc(sz);

	if (pbuf) {
		memset(pbuf, 0, sz);
	}

	return pbuf;
}

void _rtw_mfree(void *pbuf, u32 sz)
{

#ifdef RTK_DMP_PLATFORM
	if (sz > 0x4000)
		dvr_free(pbuf);
	else
#endif
		kfree(pbuf);
}

inline struct sk_buff *_rtw_skb_alloc(u32 sz)
{
	return __dev_alloc_skb(sz, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
}

inline struct sk_buff *_rtw_skb_copy(const struct sk_buff *skb)
{
	return skb_copy(skb, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
}

inline struct sk_buff *_rtw_skb_clone(struct sk_buff *skb)
{
	return skb_clone(skb, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
}
inline struct sk_buff *_rtw_pskb_copy(struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	return pskb_copy(skb, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
#else
	return skb_clone(skb, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
#endif
}

inline int _rtw_netif_rx(struct  net_device * ndev, struct sk_buff *skb)
{
	skb->dev = ndev;
	return netif_rx(skb);
}

#ifdef CONFIG_RTW_NAPI
inline int _rtw_netif_receive_skb(struct  net_device * ndev, struct sk_buff *skb)
{
	skb->dev = ndev;
	return netif_receive_skb(skb);
}

inline gro_result_t _rtw_napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	return napi_gro_receive(napi, skb);
}
#endif /* CONFIG_RTW_NAPI */

void _rtw_skb_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(list)))
		dev_kfree_skb_any(skb);
}

inline void *_rtw_usb_buffer_alloc(struct usb_device *dev, size_t size, dma_addr_t *dma)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	return usb_alloc_coherent(dev, size, (in_interrupt() ? GFP_ATOMIC : GFP_KERNEL), dma);
#else
	return usb_buffer_alloc(dev, size, (in_interrupt() ? GFP_ATOMIC : GFP_KERNEL), dma);
#endif
}
inline void _rtw_usb_buffer_free(struct usb_device *dev, size_t size, void *addr, dma_addr_t dma)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	usb_free_coherent(dev, size, addr, dma);
#else
	usb_buffer_free(dev, size, addr, dma);
#endif
}

void *rtw_malloc2d(int h, int w, size_t size)
{
	int j;

	void **a = (void **) rtw_zmalloc(h * sizeof(void *) + h * w * size);
	if (!a) {
		RTW_INFO("%s: alloc memory fail!\n", __func__);
		return NULL;
	}

	for (j = 0; j < h; j++)
		a[j] = ((char *)(a + h)) + j * w * size;

	return a;
}

void rtw_mfree2d(void *pbuf, int h, int w, int size)
{
	rtw_mfree((u8 *)pbuf, h * sizeof(void *) + w * h * size);
}

/*
For the following list_xxx operations,
caller must guarantee the atomic context.
Otherwise, there will be racing condition.
*/
u32	rtw_is_list_empty(struct list_head *phead)
{
	if (list_empty(phead))
		return true;
	else
		return false;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void rtw_init_timer(struct timer_list *ptimer, void *adapt, void *pfunc, void *ctx)
{
	struct adapter *adapter = (struct adapter *)adapt;

	_init_timer(ptimer, adapter->pnetdev, pfunc, ctx);
}
#endif

/*

Caller must check if the list is empty before calling rtw_list_delete

*/

u32 _rtw_down_sema(struct semaphore *sema)
{
	if (down_interruptible(sema))
		return _FAIL;
	else
		return _SUCCESS;
}

inline void thread_exit(struct completion *comp)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0))
	complete_and_exit(comp, 0);
#else
	kthread_complete_and_exit(comp, 0);
#endif
}

void	_rtw_mutex_init(_mutex *pmutex)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	mutex_init(pmutex);
#else
	init_MUTEX(pmutex);
#endif
}

void	_rtw_mutex_free(_mutex *pmutex);
void	_rtw_mutex_free(_mutex *pmutex)
{

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	mutex_destroy(pmutex);
#endif
}

void _rtw_init_queue(struct __queue *pqueue)
{
	INIT_LIST_HEAD(&(pqueue->queue));
	spin_lock_init(&(pqueue->lock));
}

u32	  _rtw_queue_empty(struct __queue	*pqueue)
{
	return rtw_is_list_empty(&(pqueue->queue));
}


u32 rtw_end_of_queue_search(struct list_head *head, struct list_head *plist)
{
	if (head == plist)
		return true;
	else
		return false;
}


unsigned long _rtw_get_current_time(void)
{
	return jiffies;
}

inline u32 _rtw_systime_to_ms(unsigned long stime)
{
	return jiffies_to_msecs(stime);
}

inline unsigned long _rtw_ms_to_systime(u32 ms)
{
	return msecs_to_jiffies(ms);
}

/* the input parameter start use the same unit as returned by rtw_get_current_time */
inline int _rtw_get_passing_time_ms(unsigned long start)
{
	return _rtw_systime_to_ms(_rtw_get_current_time() - start);
}

inline int _rtw_get_time_interval_ms(unsigned long start, unsigned long end)
{
	return _rtw_systime_to_ms(end - start);
}

void rtw_sleep_schedulable(int ms)
{
	u32 delta;

	delta = (ms * HZ) / 1000; /* (ms) */
	if (delta == 0) {
		delta = 1;/* 1 ms */
	}
	set_current_state(TASK_INTERRUPTIBLE);
	if (schedule_timeout(delta) != 0)
		return ;
	return;
}


void rtw_msleep_os(int ms)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	if (ms < 20) {
		unsigned long us = ms * 1000UL;
		usleep_range(us, us + 1000UL);
	} else
#endif
		msleep((unsigned int)ms);
}

void rtw_usleep_os(int us)
{
	/* msleep((unsigned int)us); */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	usleep_range(us, us + 1);
#else
	if (1 < (us / 1000))
		msleep(1);
	else
		msleep((us / 1000) + 1);
#endif
}


void rtw_mdelay_os(int ms)
{
	mdelay((unsigned long)ms);
}

void rtw_udelay_os(int us)
{
	udelay((unsigned long)us);
}

void rtw_yield_os(void)
{
	yield();
}

#define RTW_SUSPEND_LOCK_NAME "rtw_wifi"
#define RTW_SUSPEND_EXT_LOCK_NAME "rtw_wifi_ext"
#define RTW_SUSPEND_RX_LOCK_NAME "rtw_wifi_rx"
#define RTW_SUSPEND_TRAFFIC_LOCK_NAME "rtw_wifi_traffic"
#define RTW_SUSPEND_RESUME_LOCK_NAME "rtw_wifi_resume"
#define RTW_RESUME_SCAN_LOCK_NAME "rtw_wifi_scan"
#ifdef CONFIG_WAKELOCK
static struct wake_lock rtw_suspend_lock;
static struct wake_lock rtw_suspend_ext_lock;
static struct wake_lock rtw_suspend_rx_lock;
static struct wake_lock rtw_suspend_traffic_lock;
static struct wake_lock rtw_suspend_resume_lock;
static struct wake_lock rtw_resume_scan_lock;
#endif

inline void rtw_suspend_lock_init(void)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_init(&rtw_suspend_lock, WAKE_LOCK_SUSPEND, RTW_SUSPEND_LOCK_NAME);
	wake_lock_init(&rtw_suspend_ext_lock, WAKE_LOCK_SUSPEND, RTW_SUSPEND_EXT_LOCK_NAME);
	wake_lock_init(&rtw_suspend_rx_lock, WAKE_LOCK_SUSPEND, RTW_SUSPEND_RX_LOCK_NAME);
	wake_lock_init(&rtw_suspend_traffic_lock, WAKE_LOCK_SUSPEND, RTW_SUSPEND_TRAFFIC_LOCK_NAME);
	wake_lock_init(&rtw_suspend_resume_lock, WAKE_LOCK_SUSPEND, RTW_SUSPEND_RESUME_LOCK_NAME);
	wake_lock_init(&rtw_resume_scan_lock, WAKE_LOCK_SUSPEND, RTW_RESUME_SCAN_LOCK_NAME);
#endif
}

inline void rtw_suspend_lock_uninit(void)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_destroy(&rtw_suspend_lock);
	wake_lock_destroy(&rtw_suspend_ext_lock);
	wake_lock_destroy(&rtw_suspend_rx_lock);
	wake_lock_destroy(&rtw_suspend_traffic_lock);
	wake_lock_destroy(&rtw_suspend_resume_lock);
	wake_lock_destroy(&rtw_resume_scan_lock);
#endif
}

inline void rtw_lock_suspend(void)
{
#ifdef CONFIG_WAKELOCK
	wake_lock(&rtw_suspend_lock);
#endif
}

inline void rtw_unlock_suspend(void)
{
#ifdef CONFIG_WAKELOCK
	wake_unlock(&rtw_suspend_lock);
#endif
}

inline void rtw_resume_lock_suspend(void)
{
#ifdef CONFIG_WAKELOCK
	wake_lock(&rtw_suspend_resume_lock);
#endif
}

inline void rtw_resume_unlock_suspend(void)
{
#ifdef CONFIG_WAKELOCK
	wake_unlock(&rtw_suspend_resume_lock);
#endif
}

inline void rtw_lock_suspend_timeout(u32 timeout_ms)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_timeout(&rtw_suspend_lock, rtw_ms_to_systime(timeout_ms));
#endif
}

inline void rtw_lock_ext_suspend_timeout(u32 timeout_ms)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_timeout(&rtw_suspend_ext_lock, rtw_ms_to_systime(timeout_ms));
#endif
}

inline void rtw_lock_rx_suspend_timeout(u32 timeout_ms)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_timeout(&rtw_suspend_rx_lock, rtw_ms_to_systime(timeout_ms));
#endif
}


inline void rtw_lock_traffic_suspend_timeout(u32 timeout_ms)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_timeout(&rtw_suspend_traffic_lock, rtw_ms_to_systime(timeout_ms));
#endif
	/* RTW_INFO("traffic lock timeout:%d\n", timeout_ms); */
}

inline void rtw_lock_resume_scan_timeout(u32 timeout_ms)
{
#ifdef CONFIG_WAKELOCK
	wake_lock_timeout(&rtw_resume_scan_lock, rtw_ms_to_systime(timeout_ms));
#endif
	/* RTW_INFO("resume scan lock:%d\n", timeout_ms); */
}

inline void ATOMIC_SET(ATOMIC_T *v, int i)
{
	atomic_set(v, i);
}

inline int ATOMIC_READ(ATOMIC_T *v)
{
	return atomic_read(v);
}

inline void ATOMIC_ADD(ATOMIC_T *v, int i)
{
	atomic_add(i, v);
}
inline void ATOMIC_SUB(ATOMIC_T *v, int i)
{
	atomic_sub(i, v);
}

inline void ATOMIC_INC(ATOMIC_T *v)
{
	atomic_inc(v);
}

inline void ATOMIC_DEC(ATOMIC_T *v)
{
	atomic_dec(v);
}

inline int ATOMIC_ADD_RETURN(ATOMIC_T *v, int i)
{
	return atomic_add_return(i, v);
}

inline int ATOMIC_SUB_RETURN(ATOMIC_T *v, int i)
{
	return atomic_sub_return(i, v);
}

inline int ATOMIC_INC_RETURN(ATOMIC_T *v)
{
	return atomic_inc_return(v);
}

inline int ATOMIC_DEC_RETURN(ATOMIC_T *v)
{
	return atomic_dec_return(v);
}


/*
* Open a file with the specific @param path, @param flag, @param mode
* @param fpp the pointer of struct file pointer to get struct file pointer while file opening is success
* @param path the path of the file to open
* @param flag file operation flags, please refer to linux document
* @param mode please refer to linux document
* @return Linux specific error code
*/
static int openFile(struct file **fpp, const char *path, int flag, int mode)
{
	struct file *fp;

	fp = filp_open(path, flag, mode);
	if (IS_ERR(fp)) {
		*fpp = NULL;
		return PTR_ERR(fp);
	} else {
		*fpp = fp;
		return 0;
	}
}

/*
* Close the file with the specific @param fp
* @param fp the pointer of struct file to close
* @return always 0
*/
static int closeFile(struct file *fp)
{
	filp_close(fp, NULL);
	return 0;
}

static int readFile(struct file *fp, char *buf, int len)
{
	int rlen = 0, sum = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	if (!(fp->f_mode & FMODE_CAN_READ))
#else
	if (!fp->f_op || !fp->f_op->read)
#endif
		return -EPERM;

	while (sum < len) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
		rlen = kernel_read(fp, buf + sum, len - sum, &fp->f_pos);
#else
		rlen = __vfs_read(fp, buf + sum, len - sum, &fp->f_pos);
#endif
#else
		rlen = fp->f_op->read(fp, buf + sum, len - sum, &fp->f_pos);
#endif
		if (rlen > 0)
			sum += rlen;
		else if (0 != rlen)
			return rlen;
		else
			break;
	}

	return  sum;

}

static int writeFile(struct file *fp, char *buf, int len)
{
	int wlen = 0, sum = 0;

	if (!fp->f_op || !fp->f_op->write)
		return -EPERM;

	while (sum < len) {
		wlen = fp->f_op->write(fp, buf + sum, len - sum, &fp->f_pos);
		if (wlen > 0)
			sum += wlen;
		else if (0 != wlen)
			return wlen;
		else
			break;
	}

	return sum;

}

/*
* Test if the specifi @param path is a file and readable
* If readable, @param sz is got
* @param path the path of the file to test
* @return Linux specific error code
*/
static int isFileReadable(const char *path, u32 *sz)
{
	struct file *fp;
	int ret = 0;
#ifdef set_fs
	mm_segment_t oldfs;
#endif
	char buf;

	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp))
		ret = PTR_ERR(fp);
	else {
#ifdef set_fs
		oldfs = get_fs();
		set_fs(KERNEL_DS);
#endif

		if (1 != readFile(fp, &buf, 1))
			ret = PTR_ERR(fp);

		if (ret == 0 && sz) {
			#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
			*sz = i_size_read(fp->f_path.dentry->d_inode);
			#else
			*sz = i_size_read(fp->f_dentry->d_inode);
			#endif
		}

#ifdef set_fs
		set_fs(oldfs);
#endif
		filp_close(fp, NULL);
	}
	return ret;
}

/*
* Open the file with @param path and retrive the file content into memory starting from @param buf for @param sz at most
* @param path the path of the file to open and read
* @param buf the starting address of the buffer to store file content
* @param sz how many bytes to read at most
* @return the byte we've read, or Linux specific error code
*/
static int retriveFromFile(const char *path, u8 *buf, u32 sz)
{
	int ret = -1;
#ifdef set_fs
	mm_segment_t oldfs;
#endif
	struct file *fp;

	if (path && buf) {
		ret = openFile(&fp, path, O_RDONLY, 0);
		if (0 == ret) {
			RTW_INFO("%s openFile path:%s fp=%p\n", __func__, path , fp);

#ifdef set_fs
			oldfs = KERNEL_DS;
			set_fs(KERNEL_DS);
#endif
			ret = readFile(fp, buf, sz);
#ifdef set_fs
			set_fs(oldfs);
#endif
			closeFile(fp);

			RTW_INFO("%s readFile, ret:%d\n", __func__, ret);

		} else
			RTW_INFO("%s openFile path:%s Fail, ret:%d\n", __func__, path, ret);
	} else {
		RTW_INFO("%s NULL pointer\n", __func__);
		ret =  -EINVAL;
	}
	return ret;
}

/*
* Open the file with @param path and wirte @param sz byte of data starting from @param buf into the file
* @param path the path of the file to open and write
* @param buf the starting address of the data to write into file
* @param sz how many bytes to write at most
* @return the byte we've written, or Linux specific error code
*/
static int storeToFile(const char *path, u8 *buf, u32 sz)
{
	int ret = 0;
#ifdef set_fs
	mm_segment_t oldfs;
#endif
	struct file *fp;

	if (path && buf) {
		ret = openFile(&fp, path, O_CREAT | O_WRONLY, 0666);
		if (0 == ret) {
			RTW_INFO("%s openFile path:%s fp=%p\n", __func__, path , fp);

#ifdef set_fs
			oldfs = get_fs();
			set_fs(KERNEL_DS);
#endif
			ret = writeFile(fp, buf, sz);
#ifdef set_fs
			set_fs(oldfs);
#endif
			closeFile(fp);

			RTW_INFO("%s writeFile, ret:%d\n", __func__, ret);

		} else
			RTW_INFO("%s openFile path:%s Fail, ret:%d\n", __func__, path, ret);
	} else {
		RTW_INFO("%s NULL pointer\n", __func__);
		ret =  -EINVAL;
	}
	return ret;
}

/*
* Test if the specifi @param path is a file and readable
* @param path the path of the file to test
* @return true or false
*/
int rtw_is_file_readable(const char *path)
{
	if (isFileReadable(path, NULL) == 0)
		return true;
	else
		return false;
}

/*
* Test if the specifi @param path is a file and readable.
* If readable, @param sz is got
* @param path the path of the file to test
* @return true or false
*/
int rtw_is_file_readable_with_size(const char *path, u32 *sz)
{
	if (isFileReadable(path, sz) == 0)
		return true;
	else
		return false;
}

/*
* Open the file with @param path and retrive the file content into memory starting from @param buf for @param sz at most
* @param path the path of the file to open and read
* @param buf the starting address of the buffer to store file content
* @param sz how many bytes to read at most
* @return the byte we've read
*/
int rtw_retrieve_from_file(const char *path, u8 *buf, u32 sz)
{
	int ret = retriveFromFile(path, buf, sz);
	return ret >= 0 ? ret : 0;
}

/*
* Open the file with @param path and wirte @param sz byte of data starting from @param buf into the file
* @param path the path of the file to open and write
* @param buf the starting address of the data to write into file
* @param sz how many bytes to write at most
* @return the byte we've written
*/
int rtw_store_to_file(const char *path, u8 *buf, u32 sz)
{
	int ret = storeToFile(path, buf, sz);
	return ret >= 0 ? ret : 0;
}

struct net_device *rtw_alloc_etherdev_with_old_priv(int sizeof_priv, void *old_priv)
{
	struct net_device *pnetdev;
	struct rtw_netdev_priv_indicator *pnpi;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	pnetdev = alloc_etherdev_mq(sizeof(struct rtw_netdev_priv_indicator), 4);
#else
	pnetdev = alloc_etherdev(sizeof(struct rtw_netdev_priv_indicator));
#endif
	if (!pnetdev)
		goto RETURN;

	pnpi = netdev_priv(pnetdev);
	pnpi->priv = old_priv;
	pnpi->sizeof_priv = sizeof_priv;

RETURN:
	return pnetdev;
}

struct net_device *rtw_alloc_etherdev(int sizeof_priv)
{
	struct net_device *pnetdev;
	struct rtw_netdev_priv_indicator *pnpi;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	pnetdev = alloc_etherdev_mq(sizeof(struct rtw_netdev_priv_indicator), 4);
#else
	pnetdev = alloc_etherdev(sizeof(struct rtw_netdev_priv_indicator));
#endif
	if (!pnetdev)
		goto RETURN;

	pnpi = netdev_priv(pnetdev);

	pnpi->priv = vzalloc(sizeof_priv);
	if (!pnpi->priv) {
		free_netdev(pnetdev);
		pnetdev = NULL;
		goto RETURN;
	}

	pnpi->sizeof_priv = sizeof_priv;
RETURN:
	return pnetdev;
}

void rtw_free_netdev(struct net_device *netdev)
{
	struct rtw_netdev_priv_indicator *pnpi;

	if (!netdev)
		goto RETURN;

	pnpi = netdev_priv(netdev);

	if (!pnpi->priv)
		goto RETURN;

	free_netdev(netdev);

RETURN:
	return;
}

int rtw_change_ifname(struct adapter *adapt, const char *ifname)
{
	struct dvobj_priv *dvobj;
	struct net_device *pnetdev;
	struct net_device *cur_pnetdev;
	struct rereg_nd_name_data *rereg_priv;
	int ret;
	u8 rtnl_lock_needed;

	if (!adapt)
		goto error;

	dvobj = adapter_to_dvobj(adapt);
	cur_pnetdev = adapt->pnetdev;
	rereg_priv = &adapt->rereg_nd_name_priv;

	/* free the old_pnetdev */
	if (rereg_priv->old_pnetdev) {
		free_netdev(rereg_priv->old_pnetdev);
		rereg_priv->old_pnetdev = NULL;
	}

	rtnl_lock_needed = rtw_rtnl_lock_needed(dvobj);

	if (rtnl_lock_needed)
		unregister_netdev(cur_pnetdev);
	else
		unregister_netdevice(cur_pnetdev);

	rereg_priv->old_pnetdev = cur_pnetdev;

	pnetdev = rtw_init_netdev(adapt);
	if (!pnetdev)  {
		ret = -1;
		goto error;
	}

	SET_NETDEV_DEV(pnetdev, dvobj_to_dev(adapter_to_dvobj(adapt)));

	rtw_init_netdev_name(pnetdev, ifname);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
	memcpy(pnetdev->dev_addr, adapter_mac_addr(adapt), ETH_ALEN);
#else
	dev_addr_set(pnetdev, adapter_mac_addr(adapt));
#endif
	if (rtnl_lock_needed)
		ret = register_netdev(pnetdev);
	else
		ret = register_netdevice(pnetdev);

	if (ret != 0) {
		goto error;
	}

	return 0;

error:

	return -1;

}

#ifdef CONFIG_PLATFORM_SPRD
	#ifdef do_div
		#undef do_div
	#endif
	#include <asm-generic/div64.h>
#endif

u64 rtw_modular64(u64 x, u64 y)
{
	return do_div(x, y);
}

u64 rtw_division64(u64 x, u64 y)
{
	do_div(x, y);
	return x;
}

inline u32 rtw_random32(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	return get_random_u32();
#else
	return prandom_u32();
#endif
#elif (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18))
	u32 random_int;
	get_random_bytes(&random_int , 4);
	return random_int;
#else
	return random32();
#endif
}

void rtw_buf_free(u8 **buf, u32 *buf_len)
{
	u32 ori_len;

	if (!buf || !buf_len)
		return;

	ori_len = *buf_len;

	if (*buf) {
		u32 tmp_buf_len = *buf_len;
		*buf_len = 0;
		rtw_mfree(*buf, tmp_buf_len);
		*buf = NULL;
	}
}

void rtw_buf_update(u8 **buf, u32 *buf_len, u8 *src, u32 src_len)
{
	u32 ori_len = 0, dup_len = 0;
	u8 *ori = NULL;
	u8 *dup = NULL;

	if (!buf || !buf_len)
		return;

	if (!src || !src_len)
		goto keep_ori;

	/* duplicate src */
	dup = rtw_malloc(src_len);
	if (dup) {
		dup_len = src_len;
		memcpy(dup, src, dup_len);
	}

keep_ori:
	ori = *buf;
	ori_len = *buf_len;

	/* replace buf with dup */
	*buf_len = 0;
	*buf = dup;
	*buf_len = dup_len;

	/* free ori */
	if (ori && ori_len > 0)
		rtw_mfree(ori, ori_len);
}


/**
 * rtw_cbuf_full - test if cbuf is full
 * @cbuf: pointer of struct rtw_cbuf
 *
 * Returns: true if cbuf is full
 */
inline bool rtw_cbuf_full(struct rtw_cbuf *cbuf)
{
	return (cbuf->write == cbuf->read - 1) ? true : false;
}

/**
 * rtw_cbuf_empty - test if cbuf is empty
 * @cbuf: pointer of struct rtw_cbuf
 *
 * Returns: true if cbuf is empty
 */
inline bool rtw_cbuf_empty(struct rtw_cbuf *cbuf)
{
	return (cbuf->write == cbuf->read) ? true : false;
}

/**
 * rtw_cbuf_push - push a pointer into cbuf
 * @cbuf: pointer of struct rtw_cbuf
 * @buf: pointer to push in
 *
 * Lock free operation, be careful of the use scheme
 * Returns: true push success
 */
bool rtw_cbuf_push(struct rtw_cbuf *cbuf, void *buf)
{
	if (rtw_cbuf_full(cbuf))
		return _FAIL;

	cbuf->bufs[cbuf->write] = buf;
	cbuf->write = (cbuf->write + 1) % cbuf->size;

	return _SUCCESS;
}

/**
 * rtw_cbuf_pop - pop a pointer from cbuf
 * @cbuf: pointer of struct rtw_cbuf
 *
 * Lock free operation, be careful of the use scheme
 * Returns: pointer popped out
 */
void *rtw_cbuf_pop(struct rtw_cbuf *cbuf)
{
	void *buf;
	if (rtw_cbuf_empty(cbuf))
		return NULL;

	buf = cbuf->bufs[cbuf->read];
	cbuf->read = (cbuf->read + 1) % cbuf->size;

	return buf;
}

/**
 * rtw_cbuf_alloc - allocte a rtw_cbuf with given size and do initialization
 * @size: size of pointer
 *
 * Returns: pointer of srtuct rtw_cbuf, NULL for allocation failure
 */
struct rtw_cbuf *rtw_cbuf_alloc(u32 size)
{
	struct rtw_cbuf *cbuf;

	cbuf = (struct rtw_cbuf *)rtw_malloc(sizeof(*cbuf) + sizeof(void *) * size);

	if (cbuf) {
		cbuf->write = cbuf->read = 0;
		cbuf->size = size;
	}

	return cbuf;
}

/**
 * rtw_cbuf_free - free the given rtw_cbuf
 * @cbuf: pointer of struct rtw_cbuf to free
 */
void rtw_cbuf_free(struct rtw_cbuf *cbuf)
{
	rtw_mfree((u8 *)cbuf, sizeof(*cbuf) + sizeof(void *) * cbuf->size);
}

/**
 * map_readN - read a range of map data
 * @map: map to read
 * @offset: start address to read
 * @len: length to read
 * @buf: pointer of buffer to store data read
 *
 * Returns: _SUCCESS or _FAIL
 */
int map_readN(const struct map_t *map, u16 offset, u16 len, u8 *buf)
{
	const struct map_seg_t *seg;
	int ret = _FAIL;
	int i;

	if (len == 0) {
		rtw_warn_on(1);
		goto exit;
	}

	if (offset + len > map->len) {
		rtw_warn_on(1);
		goto exit;
	}

	memset(buf, map->init_value, len);

	for (i = 0; i < map->seg_num; i++) {
		u8 *c_dst, *c_src;
		u16 c_len;

		seg = map->segs + i;
		if (seg->sa + seg->len <= offset || seg->sa >= offset + len)
			continue;

		if (seg->sa >= offset) {
			c_dst = buf + (seg->sa - offset);
			c_src = seg->c;
			if (seg->sa + seg->len <= offset + len)
				c_len = seg->len;
			else
				c_len = offset + len - seg->sa;
		} else {
			c_dst = buf;
			c_src = seg->c + (offset - seg->sa);
			if (seg->sa + seg->len >= offset + len)
				c_len = len;
			else
				c_len = seg->sa + seg->len - offset;
		}
			
		memcpy(c_dst, c_src, c_len);
	}

exit:
	return ret;
}

/**
 * map_read8 - read 1 byte of map data
 * @map: map to read
 * @offset: address to read
 *
 * Returns: value of data of specified offset. map.init_value if offset is out of range
 */
u8 map_read8(const struct map_t *map, u16 offset)
{
	const struct map_seg_t *seg;
	u8 val = map->init_value;
	int i;

	if (offset + 1 > map->len) {
		rtw_warn_on(1);
		goto exit;
	}

	for (i = 0; i < map->seg_num; i++) {
		seg = map->segs + i;
		if (seg->sa + seg->len <= offset || seg->sa >= offset + 1)
			continue;

		val = *(seg->c + offset - seg->sa);
		break;
	}

exit:
	return val;
}

/**
* is_null -
*
* Return	true if c is null character
*		false otherwise.
*/
inline bool is_null(char c)
{
	if (c == '\0')
		return true;
	else
		return false;
}

inline bool is_all_null(char *c, int len)
{
	for (; len > 0; len--)
		if (c[len - 1] != '\0')
			return false;

	return true;
}

/**
* is_eol -
*
* Return	true if c is represent for EOL (end of line)
*		false otherwise.
*/
inline bool is_eol(char c)
{
	if (c == '\r' || c == '\n')
		return true;
	else
		return false;
}

/**
* is_space -
*
* Return	true if c is represent for space
*		false otherwise.
*/
inline bool is_space(char c)
{
	if (c == ' ' || c == '\t')
		return true;
	else
		return false;
}

/**
* IsHexDigit -
*
* Return	true if chTmp is represent for hex digit
*		false otherwise.
*/
inline bool IsHexDigit(char chTmp)
{
	if ((chTmp >= '0' && chTmp <= '9') ||
		(chTmp >= 'a' && chTmp <= 'f') ||
		(chTmp >= 'A' && chTmp <= 'F'))
		return true;
	else
		return false;
}

/**
* is_alpha -
*
* Return	true if chTmp is represent for alphabet
*		false otherwise.
*/
inline bool is_alpha(char chTmp)
{
	if ((chTmp >= 'a' && chTmp <= 'z') ||
		(chTmp >= 'A' && chTmp <= 'Z'))
		return true;
	else
		return false;
}

inline char alpha_to_upper(char c)
{
	if ((c >= 'a' && c <= 'z'))
		c = 'A' + (c - 'a');
	return c;
}

int hex2num_i(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int hex2byte_i(const char *hex)
{
	int a, b;
	a = hex2num_i(*hex++);
	if (a < 0)
		return -1;
	b = hex2num_i(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, u8 *buf, size_t len)
{
	size_t i;
	int a;
	const char *ipos = hex;
	u8 *opos = buf;

	for (i = 0; i < len; i++) {
		a = hex2byte_i(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}
	return 0;
}

