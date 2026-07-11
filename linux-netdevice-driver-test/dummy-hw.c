// dummy-hw.c
/*
 * Simulated "hardware" backend for the dummy ethernet driver.
 *
 * The eth driver hands us TX skbs via lt_hw_tx(); a kernel thread plays the
 * role of the device and, a short while later, raises a fake "RX interrupt"
 * that loops the frame back into the eth driver's RX callback.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include "dummy.h"

#define DHW_NAME "dummy-hw"

/* Queue of frames pending "transmission" by the fake HW. */
static struct sk_buff_head skb_tx_q;

/* RX callback registered by the eth driver. */
static int32_t (*dummy_drv_rx)(struct sk_buff *skb);

/* The kthread that emulates the device. */
static struct task_struct *hw_thread_id;

static void dummy_hw_send_irq(void)
{
    struct sk_buff *skb = skb_dequeue(&skb_tx_q);

    if (skb == NULL) {
        printk(KERN_ERR "DHW: unable to dequeue TX skb\n");
        return;
    }

    printk(KERN_ERR "DHW: sending RX interrupt for skb=%p\n", skb);
    if (dummy_drv_rx == NULL) {
        printk(KERN_ERR "DHW: ASL interrupt not requested freeing\n");
        dev_kfree_skb(skb);
    } else {
        dummy_drv_rx(skb);
    }
}

static int dummy_hw_thread(void *arg)
{
    printk(KERN_INFO "DHW thread started: %p ...\n", current);

    while (!kthread_should_stop()) {
        /* Going to sleep for 10ms to simulate hardware processing */
        msleep(10);

        /* Do TX (send) here */
        while (!skb_queue_empty(&skb_tx_q))
            dummy_hw_send_irq();
    }

    printk(KERN_INFO "DHW thread closed\n");
    return 0;
}

int lt_hw_tx(struct sk_buff *skb)
{
    printk(KERN_ERR "DHW: adding skb=%p into HW Q\n", skb);
    /* Insert into tail and pick from head */
    skb_queue_tail(&skb_tx_q, skb);

    return 0;
}
EXPORT_SYMBOL_GPL(lt_hw_tx);

int32_t lt_request_irq(bool mode, int32_t (*rx_fn)(struct sk_buff *skb))
{
    if ((mode == true) && (dummy_drv_rx == NULL)) {
        printk(KERN_ERR "DHW: ASL register IRQ for RX\n");
        dummy_drv_rx = rx_fn;
    } else if ((mode == false) && (dummy_drv_rx != NULL)) {
        printk(KERN_ERR "DHW: ASL deregister IRQ for RX\n");
        dummy_drv_rx = NULL;
        /* Flush anything still queued so we do not touch a freed callback. */
        skb_queue_purge(&skb_tx_q);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(lt_request_irq);

static int __init dummy_hw_init(void)
{
    printk(KERN_INFO "DHW: module init\n");

    skb_queue_head_init(&skb_tx_q);
    dummy_drv_rx = NULL;

    hw_thread_id = kthread_run(dummy_hw_thread, NULL, DHW_NAME);
    if (IS_ERR(hw_thread_id)) {
        printk(KERN_ERR "DHW: failed to start HW thread\n");
        return PTR_ERR(hw_thread_id);
    }

    return 0;
}

static void __exit dummy_hw_exit(void)
{
    printk(KERN_INFO "DHW: module exit\n");

    if (!IS_ERR_OR_NULL(hw_thread_id))
        kthread_stop(hw_thread_id);

    skb_queue_purge(&skb_tx_q);
}

module_init(dummy_hw_init);
module_exit(dummy_hw_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simulated HW backend for the dummy ethernet driver");
MODULE_VERSION("1.0");
