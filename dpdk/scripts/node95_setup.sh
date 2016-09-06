#!/bin/bash

if [ "$EUID" -ne 0 ] ;  then
	echo "Please run as root"
	echo "sudo -E ./setup.sh"
	exit -1
fi

echo 2048 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
echo 2048 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages

modprobe uio_pci_generic

sudo ip link set dev eth2 down

$RTE_SDK/tools/dpdk_nic_bind.py --status
$RTE_SDK/tools/dpdk_nic_bind.py --bind=uio_pci_generic 01:00.0
$RTE_SDK/tools/dpdk_nic_bind.py --bind=uio_pci_generic 01:00.1
$RTE_SDK/tools/dpdk_nic_bind.py --status
