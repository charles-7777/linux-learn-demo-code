#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#define PROC_DIR_NAME "proc_demo"
#define PROC_FILE_NAME "status"
#define BUF_SIZE 1024

static char kernel_buffer[BUF_SIZE];

// 回调函数：读取文件内容
static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Message from kernel: %s\n", kernel_buffer);
    return 0;
}

// 回调函数：打开文件
static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

// 回调函数：向文件写入数据
static ssize_t proc_write(struct file *file, const char __user *buf, 
                          size_t count, loff_t *ppos)
{
    size_t len = (count >= BUF_SIZE) ? BUF_SIZE - 1 : count;

    if (copy_from_user(kernel_buffer, buf, len))
        return -EFAULT;

    kernel_buffer[len] = '\0';
    printk(KERN_INFO "proc_demo: Received from user space: %s\n", kernel_buffer);

    return len;
}

// 文件操作结构体
static const struct proc_ops proc_fops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_write   = proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init proc_demo_init(void)
{
    struct proc_dir_entry *dir, *file;

    // 1. 创建 /proc/proc_demo 目录
    dir = proc_mkdir(PROC_DIR_NAME, NULL);
    if (!dir)
        return -ENOMEM;

    // 2. 在目录下创建 /proc/proc_demo/status 文件
    file = proc_create(PROC_FILE_NAME, 0666, dir, &proc_fops);
    if (!file) {
        remove_proc_entry(PROC_DIR_NAME, NULL); // 清理已创建的目录
        return -ENOMEM;
    }

    // 初始化默认消息
    strcpy(kernel_buffer, "Hello from /proc/proc_demo/status!");

    printk(KERN_INFO "proc_demo: Directory /proc/%s created successfully\n", PROC_DIR_NAME);
    return 0;
}

static void __exit proc_demo_exit(void)
{
    // 3. 安全地删除整个目录及其下的所有子文件
    remove_proc_subtree(PROC_DIR_NAME, NULL);
    printk(KERN_INFO "proc_demo: /proc/%s removed\n", PROC_DIR_NAME);
}

module_init(proc_demo_init);
module_exit(proc_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Test Author");
MODULE_DESCRIPTION("A /proc directory and file demo module");
