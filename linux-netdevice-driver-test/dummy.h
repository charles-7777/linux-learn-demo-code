/* dummy.h - shared interface between the "HW" layer and the eth driver */
#ifndef _DUMMY_H
#define _DUMMY_H

#include <linux/types.h>
#include <linux/skbuff.h>

/* Called by the eth driver to push a TX skb into the (simulated) HW queue. */
int lt_hw_tx(struct sk_buff *skb);

/*
 * Register (mode = true) / deregister (mode = false) the RX callback that the
 * HW layer invokes when it "receives" a frame.
 */
int32_t lt_request_irq(bool mode, int32_t (*rx_fn)(struct sk_buff *skb));

#endif /* _DUMMY_H */
