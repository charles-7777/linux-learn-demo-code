# dummy-eth: a pingable virtual ethernet driver

A small teaching example of a Linux netdevice driver split into two modules:

- `dummy-hw.ko` &ndash; a simulated "hardware" layer. It exposes `lt_hw_tx()` and
  `lt_request_irq()` and runs a kernel thread that loops transmitted frames
  back into the driver as "received" frames.
- `dummy-eth.ko` &ndash; the actual network interface (`deth0`). Its RX path acts
  as a **reflector**:
  - answers **ARP requests** with a synthetic MAC (`02:de:a.b.c.d`) for the
    requested IPv4 address, and
  - turns outgoing **ICMP echo requests** into **echo replies**.

Because of the reflector, once you give the interface an address you can ping
any peer in its subnet and it will answer.

## Build

Requires kernel headers for the running kernel (`linux-headers-$(uname -r)` or
your distro's equivalent) and a toolchain.

```sh
make
```

This produces `dummy-hw.ko` and `dummy-eth.ko`.

## Load

Order matters &ndash; the HW module must be loaded first because the eth module
uses its exported symbols:

```sh
sudo insmod dummy-hw.ko
sudo insmod dummy-eth.ko
```

Check that the interface appeared:

```sh
ip link show deth0
dmesg | tail
```

## Test with ping

```sh
sudo ip addr add 192.168.57.100/24 dev deth0
sudo ip link set deth0 up

# Ping any address in the subnet; the driver reflects it back:
ping -c 3 192.168.57.101
```

You should see replies. Watch the kernel log to see the ARP/ICMP handling and
the before/after hex dumps:

```sh
dmesg -w
```

## Unload

```sh
sudo ip link set deth0 down
sudo rmmod dummy-eth
sudo rmmod dummy-hw
```
