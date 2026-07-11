sudo ip  addr add 192.168.0.1/24 dev sn0
sudo ip  addr add 192.168.1.2/24 dev sn1
sudo ip  link set sn0 up
sudo ip  link set sn1 up

#
ping -c 3 192.168.0.2
ping -c 3 192.168.1.1

ip -s link show sn0

