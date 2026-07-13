#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

/* 被 sysfs 文件读写的内核变量 */
static int foo;

/* cat 文件时被调用 */
static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr,
            char *buf)
{
    return sprintf(buf, "%d\n", foo);
}

/* echo > 文件时被调用 */
static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,
             const char *buf, size_t count)
{
    int ret = kstrtoint(buf, 10, &foo);

    if (ret < 0)
        return ret;
    return count;   /* 必须返回已消费的字节数 */
}

/* 定义属性：0664 -> 允许 owner/group 写，但不允许 world-writable */
static struct kobj_attribute foo_attribute =
    __ATTR(foo, 0664, foo_show, foo_store);

static struct attribute *attrs[] = {
    &foo_attribute.attr,
    NULL,    /* 必须以 NULL 结尾 */
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *demo_kobj;

static int __init sysfs_demo_init(void)
{
    int ret;

    /* 在 /sys/kernel/ 下创建 sysfs_demo 目录 */
    demo_kobj = kobject_create_and_add("sysfs_demo", kernel_kobj);
    if (!demo_kobj)
        return -ENOMEM;

    /* 在该目录下创建属性文件 */
    ret = sysfs_create_group(demo_kobj, &attr_group);
    if (ret)
        kobject_put(demo_kobj);

    pr_info("sysfs_demo: loaded, see /sys/kernel/sysfs_demo/\n");
    return ret;
}

static void __exit sysfs_demo_exit(void)
{
    kobject_put(demo_kobj);   /* 引用归零后自动删除目录及文件 */
    pr_info("sysfs_demo: unloaded\n");
}

module_init(sysfs_demo_init);
module_exit(sysfs_demo_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("your name");
MODULE_DESCRIPTION("A minimal sysfs demo module");

