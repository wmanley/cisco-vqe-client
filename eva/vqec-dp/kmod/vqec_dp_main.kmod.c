/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Module main.
 *
 * Documents: 
 *
 *****************************************************************************/

#include "utils/vam_types.h"
#include "vqecutils/vqec_lock.h"
#include "vqec_dp_api.h"
#include "vqecutils/vqec_rpc_common.h_rpcgen"
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
#include "vqec_dp_output_shim_api.h"
#include <linux/proc_fs.h>
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <asm/io.h>

#define VQEC_DEV_NAME "vqec" 
#define VQEC_DEV_MAX_OPENERS 1
                                /* round allocation to page size */
#define VQEC_DEV_IPC_BUF_PAGES \
    ((VQEC_DEV_IPC_BUF_LEN + PAGE_SIZE - 1) / PAGE_SIZE)
#define VQEC_DEV_IPC_BUF_ALLOC_LEN (VQEC_DEV_IPC_BUF_PAGES * PAGE_SIZE)

#define VQEC_DP_DEV_LOG(err, format,...)                        \
    printk(err "<vqec-dev>" format "\n", ##__VA_ARGS__);
#define VQEC_DP_DEV_PRINT(format,...)                        \
    printk("[DBG2]" format "\n", ##__VA_ARGS__);

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
#define VQEC_DP_PROC_DIR "vqec"
#define VQEC_DP_PROC_FILE_INP_JITTER "jitter_inp"
#define VQEC_DP_PROC_FILE_OUT_JITTER "jitter_out"
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

static char s_kreq_buf[VQEC_DEV_IPC_BUF_ALLOC_LEN], 
    s_krsp_buf[VQEC_DEV_IPC_BUF_ALLOC_LEN];

typedef
struct vqec_dp_dev_data_
{
    pid_t tgid;                 /* thread-group id of the open-er */
    uid_t uid;                  /* uid of the open-er */   
    char *reqbuf;            /* IPC request [from-user] buffer pointer */
    char *rspbuf;            /* IPC response [to-user] buffer pointer */
    void *mmap_reqbuf;        /* User-virtual address for request buffer */
    void *mmap_rspbuf;        /* User-virtual address for response buffer */

} vqec_dp_dev_data_t;

static unsigned int vqec_major_devid = 0;
static unsigned int vqec_fast_sched_interval = 1;  /* 1 ms by default */
static unsigned int vqec_slow_sched_interval = 1;  /* 1 ms by default */
static struct semaphore s_vqec_dev_open_sem;

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
static struct proc_dir_entry *s_vqec_proc_dir, *s_vqec_proc_inp_jitter, 
    *s_vqec_proc_out_jitter;
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

module_param(vqec_major_devid, uint, S_IRUGO);
MODULE_PARM_DESC(vqec_major_devid, "major device number identifier");

module_param(vqec_fast_sched_interval, uint, S_IRUGO);
MODULE_PARM_DESC(vqec_fast_sched_interval, "scheduling interval (in ms) for "
                 "fast-scheduled tasks");

module_param(vqec_slow_sched_interval, uint, S_IRUGO);
MODULE_PARM_DESC(vqec_slow_sched_interval, "scheduling interval (in ms) for "
                 "slow-scheduled tasks");

#define DRIVER_AUTHOR "Cisco Systems Inc."
#define DRIVER_DESC "VQE client"

/**
 * Callbacks for test reader to register with vqec_dp.
 */
typedef struct vqec_dp_reader_callbacks_ {
    int32_t (*reader_start)(test_vqec_reader_params_t *params);
    void (*reader_stop)(void);
} vqec_dp_reader_callbacks_t;

static vqec_dp_reader_callbacks_t s_reader_callbacks;

void vqec_dp_register_reader_callbacks (
    int32_t (*reader_start)(test_vqec_reader_params_t *params),
    void (*reader_stop)(void))
{
    s_reader_callbacks.reader_start = reader_start;
    s_reader_callbacks.reader_stop = reader_stop;
}

EXPORT_SYMBOL(vqec_dp_register_reader_callbacks);

/**
 * Launch the DP test reader module, if available.
 *
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_test_reader (test_vqec_reader_params_t params)
{
    int moderr;
    int32_t reader_err = 0;

    moderr = request_module("test_vqec_reader");
    if (moderr < 0) {
        printk("ERROR: failed to load module (%d)\n", moderr);
        return (VQEC_DP_ERR_INTERNAL);
    }

    if (params.disable) {
        if (s_reader_callbacks.reader_stop) {
            /* stop the reader task */
            s_reader_callbacks.reader_stop();
        } else {
            printk("ERROR: reader_stop() not defined!\n");
            return (VQEC_DP_ERR_INTERNAL);
        }
    } else if (s_reader_callbacks.reader_start) {
        /* start the reader task */
        reader_err = s_reader_callbacks.reader_start(&params);
    } else {
        printk("ERROR: reader_start() not defined!\n");
        return (VQEC_DP_ERR_INTERNAL);
    }

    if (reader_err) {
        return (VQEC_DP_ERR_ALREADY_INITIALIZED);
    }

    return (VQEC_DP_ERR_OK);
}

/**---------------------------------------------------------------------------
 * Open vector for the device. In our current implementation the device
 * can be opened only by a single CP client. 
 *
 * @param[out] int32_t Error code - returns 0 on success.
 *---------------------------------------------------------------------------*/
static vqec_dp_dev_data_t s_dev_data;
static int32_t 
vqec_dp_dev_open (struct inode *inode, struct file *filp)
{
    uint32_t minor;
    int32_t ret = 0;
    vqec_dp_dev_data_t *data = &s_dev_data;

    minor = iminor(inode);
    if (minor) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "No such minor device %u", minor);
        return (-ENODEV);
    }

    if (!try_module_get(THIS_MODULE)) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "Failed to acquire module handle");
        return (-ENODEV);
    }

    if (down_trylock(&s_vqec_dev_open_sem)) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "The device is already open - first close it");
        module_put(THIS_MODULE);
        return  (-EWOULDBLOCK);
    }

    memset(data, 0, sizeof(*data));

    if (!(data->reqbuf = kmalloc(VQEC_DEV_IPC_BUF_ALLOC_LEN, GFP_KERNEL)) ||
        !(data->rspbuf = kmalloc(VQEC_DEV_IPC_BUF_ALLOC_LEN, GFP_KERNEL))) {
        VQEC_DP_DEV_LOG(KERN_ERR, "memory allocation failure");
        ret = -ENOMEM;
        goto done;
    }
    data->tgid = current->tgid;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
    data->uid = current_uid();
#else
    data->uid = current->uid;
#endif

    filp->private_data = data;
    
  done:
    if (ret) {
        if (data->reqbuf) {
            kfree(data->reqbuf);
        }
        if (data->rspbuf) {
            kfree(data->rspbuf);
        }
        filp->private_data = NULL;
        memset(data, 0, sizeof(*data));
        up(&s_vqec_dev_open_sem);
        module_put(THIS_MODULE);
    }
    
    return (ret);
}


/**---------------------------------------------------------------------------
 * Release vector for the device. 
 *
 * @param[out] int32_t Error code - returns 0 on success.
 *---------------------------------------------------------------------------*/
static int32_t 
vqec_dp_dev_release (struct inode *inode, struct file *filp)
{
    vqec_dp_dev_data_t *data = filp->private_data;

    if (data && data->reqbuf) {
        kfree(data->reqbuf);
    }
    if (data && data->rspbuf) {
        kfree(data->rspbuf);
    }
    filp->private_data = NULL;
    up(&s_vqec_dev_open_sem);
    module_put(THIS_MODULE);

    return (0);
}


/**---------------------------------------------------------------------------
 * MMAP vector.  If the offset is VQEC_DEV_IPC_BUF_REQ_OFFSET, the request
 * buffer is mapped. If the offset is VQEC_DEV_IPC_BUF_RSP_OFFSET, the 
 * response buffer is mapped.
 *
 * @param[out] int32_t Error code - returns 0 on success.
 *---------------------------------------------------------------------------*/
static struct vm_operations_struct vqec_dp_dev_vmops;
static int32_t 
vqec_dp_dev_mmap (struct file *filp, struct vm_area_struct *vma)
{
    uint32_t offset, size;
    vqec_dp_dev_data_t *data = filp->private_data;

    offset = vma->vm_pgoff << PAGE_SHIFT;
    if ((offset != VQEC_DEV_IPC_BUF_REQ_OFFSET) &&
        (offset != VQEC_DEV_IPC_BUF_RSP_OFFSET)) {
        VQEC_DP_DEV_LOG(KERN_ERR, "invalid mmap offset (%u)", offset);
        return (-EINVAL);
    }

    if (((offset == VQEC_DEV_IPC_BUF_REQ_OFFSET) &&
         data->mmap_reqbuf) || 
        ((offset == VQEC_DEV_IPC_BUF_RSP_OFFSET) &&
         data->mmap_rspbuf)) {
        VQEC_DP_DEV_LOG(KERN_ERR, "a previous mmap exists (%u)", offset);
        return (-EINVAL);
    }
    
    size = vma->vm_end - vma->vm_start;
    if ((size < VQEC_DEV_IPC_BUF_LEN) ||
        (size > VQEC_DEV_IPC_BUF_ALLOC_LEN)) {
        VQEC_DP_DEV_LOG(KERN_ERR, "invalid region size (%u)", size);
        return (-EINVAL);
    }    

    if (vma->vm_flags & VM_EXEC) {
        VQEC_DP_DEV_LOG(KERN_ERR, "invalid region flags (%lx)", vma->vm_flags);
        return (-EINVAL);
    }

    if (offset == VQEC_DEV_IPC_BUF_REQ_OFFSET) {
        data->mmap_reqbuf = (void *)vma->vm_start;
    } else {
        data->mmap_rspbuf = (void *)vma->vm_start;
    }
    
    vma->vm_ops = &vqec_dp_dev_vmops;
    vma->vm_flags |= VM_RESERVED;
    
    return (0);
}


/**---------------------------------------------------------------------------
 * Device-specific vma open: no implementation. 
 *---------------------------------------------------------------------------*/
static void 
vqec_dp_dev_vma_open (struct vm_area_struct* vma)
{
    return;
}


/**---------------------------------------------------------------------------
 * Device-specific vma close: no implementation. 
 *---------------------------------------------------------------------------*/
static void 
vqec_dp_dev_vma_close (struct vm_area_struct* vma)
{
    return;
}

/**---------------------------------------------------------------------------
 * Device-specific vma nopage/fault vector. This vector is invoked when
 * a page is accessed in the shared memory region, which is to this point
 * unmapped in the user process's address space. 
 *---------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
#define DEV_PAGE_PTR vmf->page
static int
vqec_dp_dev_vma_fault (struct vm_area_struct *vma,
                       struct vm_fault *vmf)
#else
#define DEV_PAGE_PTR page
static struct page* 
vqec_dp_dev_vma_nopage (struct vm_area_struct *vma, 
                        unsigned long address, int* type)
#endif
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
    unsigned long address;
#else
    struct page *page = NOPAGE_SIGBUS;
#endif
    unsigned long offset, pgprot;
    void *bufp;
    vqec_dp_dev_data_t *data = &s_dev_data;

    if (!data->reqbuf || !data->rspbuf) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
        return (VM_FAULT_SIGBUS);
#else
        return (page);
#endif
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
    address = (unsigned long)vmf->virtual_address;
#endif
    offset = ((address - vma->vm_start) >> PAGE_SHIFT) + vma->vm_pgoff;
    VQEC_DP_DEV_PRINT("address %lx, vm_start %lx, vm_pgoff %lx offset %lx",
                      address, vma->vm_start, vma->vm_pgoff, offset);

    if ((offset  >= (VQEC_DEV_IPC_BUF_REQ_OFFSET >> PAGE_SHIFT)) &&
        (offset  < (VQEC_DEV_IPC_BUF_RSP_OFFSET >> PAGE_SHIFT))) {
        bufp = (char *)data->reqbuf + 
            (offset - (VQEC_DEV_IPC_BUF_REQ_OFFSET >> PAGE_SHIFT));
        ASSERT((address - vma->vm_start) >> PAGE_SHIFT < 
               VQEC_DEV_IPC_BUF_PAGES);
        DEV_PAGE_PTR = virt_to_page(bufp);
        VQEC_DP_DEV_PRINT("map bufp %p, reqbuf %p", bufp, data->reqbuf);

    } else if (offset < ((VQEC_DEV_IPC_BUF_RSP_OFFSET + 
                          VQEC_DEV_IPC_BUF_ALLOC_LEN) >> PAGE_SHIFT)) {
        bufp = (char *)data->rspbuf + 
            (offset - (VQEC_DEV_IPC_BUF_RSP_OFFSET >> PAGE_SHIFT));
        ASSERT((address - vma->vm_start) >> PAGE_SHIFT < 
               VQEC_DEV_IPC_BUF_PAGES);
        DEV_PAGE_PTR = virt_to_page(bufp);
        VQEC_DP_DEV_PRINT("map bufp %p, rspbuf %p", bufp, data->rspbuf);

    } else {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
        return (VM_FAULT_SIGBUS);
#else
        return (page);
#endif
    }

    get_page(DEV_PAGE_PTR);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30)
    if (type) {
        *type = VM_FAULT_MINOR;
    }
#endif

    pgprot = pgprot_val(vma->vm_page_prot);
    VQEC_DP_DEV_PRINT("0x%08lx-0x%08lx %p %lx %c%c%c%c%c%c",
                      vma->vm_start,
                      vma->vm_end,
                      (offset == VQEC_DEV_IPC_BUF_REQ_OFFSET) ? 
                      data->mmap_reqbuf : data->mmap_rspbuf,
                      pgprot,
                      vma->vm_flags & VM_READ     ? 'r' : '-',
                      vma->vm_flags & VM_WRITE    ? 'w' : '-',
                      vma->vm_flags & VM_EXEC     ? 'x' : '-',
                      vma->vm_flags & VM_MAYSHARE ? 's' : 'p',
                      vma->vm_flags & VM_LOCKED   ? 'l' : '-',
                      vma->vm_flags & VM_IO       ? 'i' : '-');

#if defined(__i386__)
    VQEC_DP_DEV_PRINT("%c%c%c%c%c%c%c%c%c",
                      pgprot & _PAGE_PRESENT  ? 'p' : '-',
                      pgprot & _PAGE_RW       ? 'w' : 'r',
                      pgprot & _PAGE_USER     ? 'u' : 's',
                      pgprot & _PAGE_PWT      ? 't' : 'b',
                      pgprot & _PAGE_PCD      ? 'u' : 'c',
                      pgprot & _PAGE_ACCESSED ? 'a' : '-',
                      pgprot & _PAGE_DIRTY    ? 'd' : '-',
                      pgprot & _PAGE_PSE      ? 'm' : 'k',
                      pgprot & _PAGE_GLOBAL   ? 'g' : 'l' );
#endif

#if defined (CONFIG_CPU_SH1) || defined (CONFIG_CPU_SH2) || defined (CONFIG_CPU_SH3) || defined (CONFIG_CPU_SH4)
    VQEC_DP_DEV_PRINT("%s %s %s %s %s %s %s %s %s %s" 
#ifdef _PAGE_U0_SHARED
                      " %s"
#endif
                      ,
                      pgprot & _PAGE_WT  ? "wT" : "-",
                      pgprot & _PAGE_HW_SHARED      ? "hW" : "-",
                      pgprot & _PAGE_DIRTY     ? "dI" : "-",
                      pgprot & _PAGE_CACHABLE      ? "Ca" : "-",
                      pgprot & _PAGE_SZ0      ? "sZ" : "-",
                      pgprot & _PAGE_RW ? "rW" : "-",
                      pgprot & _PAGE_USER    ? "uS" : "-",
                      pgprot & _PAGE_PRESENT      ? "pR" : "-",
                      pgprot & _PAGE_PROTNONE   ? "pO" : "-",
                      pgprot & _PAGE_ACCESSED   ? "aC" : "-"
#ifdef _PAGE_U0_SHARED
                      ,
                      pgprot & _PAGE_U0_SHARED   ? "u0" : "-"
#endif
        );
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
        return (0);
#else
        return (page);
#endif
}


/**---------------------------------------------------------------------------
 * IOCTL vector. 
 *
 * @param[in] inode Pointer to the files' inode (unused).
 * @param[in] Pointer to the file data structure.
 * @param[in] ioctl Ioctl number.
 * @param[in] arg Ioctl argument (unused in this implementation).
 * @param[out] int32_t Error code - returns 0 on success.
 *---------------------------------------------------------------------------*/
static int32_t 
vqec_dp_dev_ioctl (struct inode *inode, struct file *filp, 
                   unsigned int ioctl, unsigned long arg)
{
    int32_t ret;
    uint32_t rsp_size;
    vqec_dp_dev_data_t *data = filp->private_data;
    __vqec_rpc_all_req_t *req = (__vqec_rpc_all_req_t *)s_kreq_buf;
    __vqec_rpc_all_rsp_t *rsp = (__vqec_rpc_all_rsp_t *)s_krsp_buf;

    if (_IOC_TYPE(ioctl) != VQEC_DEV_IPC_IOCTL_TYPE) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "Invalid device type for ioctl %u", _IOC_TYPE(ioctl));
        return (-EINVAL);
    }

    /*
     * Although almost all intel x86 caches are coherent, caches for
     * older architectures like mips and SH4 are non-coherent.
     *
     * Thus two copies of the data may exist in cache due to use of different
     *  virtual addresses between user and kernel space. For this reason, we do 
     * not directly read / write from the shared memory response / request
     * kernel aliases. Instead we use copy_xx methods to copy the data for
     * requests from the user-space into local kernel buffers, and write it 
     * back to the user response buffer from a local kernel buffer. This
     * method is very portable across linux versions and across different
     * processors / platforms.
     */
    if (_IOC_SIZE(ioctl) > VQEC_DEV_IPC_BUF_ALLOC_LEN) {
        VQEC_DP_DEV_LOG(KERN_ERR, "Invalid request length %u", 
                        _IOC_SIZE(ioctl));
        return (-EINVAL);
    }
    if (copy_from_user(req,  data->mmap_reqbuf, _IOC_SIZE(ioctl))) {
        VQEC_DP_DEV_LOG(KERN_ERR, "Unable to copy data from user");
        return (-EFAULT);
    }
    ret = vqec_rpc_server(_IOC_NR(ioctl), 
                          _IOC_SIZE(ioctl), req, rsp, &rsp_size);
    if (!ret) {
        if (rsp_size && (rsp_size <=VQEC_DEV_IPC_BUF_ALLOC_LEN)) {
            if (copy_to_user(data->mmap_rspbuf, rsp, rsp_size)) {
                VQEC_DP_DEV_LOG(KERN_ERR, "Unable to copy data to user");
                return (-EFAULT);
            }
        } else {
            VQEC_DP_DEV_LOG(KERN_ERR, "Invalid response size %u", rsp_size);
            return (-EFAULT);
        }
    }

    return (ret);
}


#ifdef HAVE_SCHED_JITTER_HISTOGRAM
/**---------------------------------------------------------------------------
 * Publish input jitter.
 *---------------------------------------------------------------------------*/
static int vqec_dp_publish_inp_jitter (char *page,
                                       char **start,
                                       off_t off,
                                       int count,
                                       int *eof,
                                       void *data)
{
    if (off > 0) {       
        return (0);
    }
    
    return (vqec_dp_output_shim_hist_publish(
                VQEC_DP_HIST_OUTPUTSHIM_INP_DELAY,
                page, 
                PAGE_SIZE));
}

/**---------------------------------------------------------------------------
 * Publish output jitter.
 *---------------------------------------------------------------------------*/
static int vqec_dp_publish_out_jitter (char *page,
                                       char **start,
                                       off_t off,
                                       int count,
                                       int *eof,
                                       void *data)
{
    if (off > 0) {       
        return (0);
    }
    
    return (vqec_dp_output_shim_hist_publish(
                VQEC_DP_HIST_OUTPUTSHIM_READER_JITTER,
                page, 
                PAGE_SIZE));
}
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

static struct file_operations vqec_dp_dev_fops =
{
    .owner      = THIS_MODULE,
    .mmap       = vqec_dp_dev_mmap,
    .ioctl      = vqec_dp_dev_ioctl,
    .open       = vqec_dp_dev_open,
    .release    = vqec_dp_dev_release
};

static struct vm_operations_struct vqec_dp_dev_vmops = {
    .open = vqec_dp_dev_vma_open,
    .close = vqec_dp_dev_vma_close,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
    .fault = vqec_dp_dev_vma_fault
#else
    .nopage = vqec_dp_dev_vma_nopage
#endif
};

/**---------------------------------------------------------------------------
 * Initialize the kernel module - invoked from insmod. The parameters
 * supported by the kernel module are described above.
 *
 * @param[out] int32_t Exit code.
 *---------------------------------------------------------------------------*/
static int32_t __init 
vqec_dp_kmod_init (void)
{
    int32_t ret;

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    s_vqec_proc_dir = proc_mkdir(VQEC_DP_PROC_DIR, NULL);
    if (!s_vqec_proc_dir) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "Failed to create proc dir entry /proc/%s", 
                        VQEC_DP_PROC_DIR);
        return (0);
    }
    s_vqec_proc_inp_jitter = create_proc_entry(VQEC_DP_PROC_FILE_INP_JITTER,
                                               0644, 
                                               s_vqec_proc_dir);
    if (!s_vqec_proc_inp_jitter) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "Failed to create proc entry /proc/%s/%s", 
                        VQEC_DP_PROC_DIR,
                        VQEC_DP_PROC_FILE_INP_JITTER);
        goto no_inp_jitter;
    }
    s_vqec_proc_inp_jitter->owner = THIS_MODULE;
    s_vqec_proc_inp_jitter->read_proc = vqec_dp_publish_inp_jitter;

    s_vqec_proc_out_jitter = create_proc_entry(VQEC_DP_PROC_FILE_OUT_JITTER,
                                               0644, 
                                               s_vqec_proc_dir);
    if (!s_vqec_proc_out_jitter) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "Failed to create proc entry /proc/%s/%s", 
                        VQEC_DP_PROC_DIR,
                        VQEC_DP_PROC_FILE_OUT_JITTER);
        goto no_out_jitter;
    }
    s_vqec_proc_out_jitter->owner = THIS_MODULE;
    s_vqec_proc_out_jitter->read_proc = vqec_dp_publish_out_jitter;
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

    if (vqec_major_devid != 0) {
        VQEC_DP_DEV_LOG(KERN_INFO, 
                        "Attempt to register vqec device with major id %d",
                        vqec_major_devid);
    }
    ret = register_chrdev(vqec_major_devid, 
                          VQEC_DEV_NAME, &vqec_dp_dev_fops);
    if (ret < 0) {
        VQEC_DP_DEV_LOG(KERN_ERR, 
                        "Unable to register device major-id %d", 
                        vqec_major_devid);
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
        goto no_device;
#else
        return (0);
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    } else {
        if (!vqec_major_devid) {
            vqec_major_devid = ret;
        }
        VQEC_DP_DEV_LOG(KERN_INFO, 
                        "Registered vqec device with major-id %d", 
                        vqec_major_devid);
    }

    /* set up the polling and scheduling intervals */
    vqec_dp_tlm_set_default_polling_interval(vqec_fast_sched_interval);
    vqec_dp_tlm_set_fast_sched_interval(vqec_fast_sched_interval);
    vqec_dp_tlm_set_slow_sched_interval(vqec_slow_sched_interval);

    vqec_lock_init(g_vqec_dp_lock);
    sema_init(&s_vqec_dev_open_sem, VQEC_DEV_MAX_OPENERS);
    return (0);

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
  no_device:
    remove_proc_entry(VQEC_DP_PROC_FILE_INP_JITTER, s_vqec_proc_dir);

  no_out_jitter:
    remove_proc_entry(VQEC_DP_PROC_FILE_OUT_JITTER, s_vqec_proc_dir);

  no_inp_jitter:
    remove_proc_entry(VQEC_DP_PROC_DIR, NULL);
    
    return (0); 
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
}


/**---------------------------------------------------------------------------
 * Cleanup the kernel module - nominally invoked from rmmod.
 *---------------------------------------------------------------------------*/
static void __exit
vqec_dp_kmod_cleanup (void)
{
    /* deinit the module in case it is still running */
    vqec_lock_lock(g_vqec_dp_lock);
    vqec_dp_deinit_module();
    vqec_lock_unlock(g_vqec_dp_lock);
    VQEC_DP_DEV_LOG(, "Deinitted vqec dataplane successfully");

    unregister_chrdev(vqec_major_devid, VQEC_DEV_NAME);
    VQEC_DP_DEV_LOG(KERN_INFO, "Removed vqec dataplane");
}

module_init(vqec_dp_kmod_init);
module_exit(vqec_dp_kmod_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("BSD");

