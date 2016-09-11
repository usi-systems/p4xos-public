#!/bin/bash


if [ "$EUID" -ne 0 ] ;  then
	echo "Please run as root"
	echo "sudo -E ./setup.sh"
	exit -1
fi

if [ -z ${RTE_SDK} ]; then
	echo "Please set \$RTE_SDK variable"
	echo "sudo -E ./setup.sh"
	exit -1
fi

$RTE_SDK/tools/dpdk_nic_bind.py --status

if [ $# -ne 2 ] ; then
	echo "Please provide the interface name and PCI address"
	echo "e.g, sudo -E ./setup.sh eth2 01:00.0"
	exit -1
fi

echo 8191 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

modprobe uio_pci_generic

sudo ip link set dev $1 down

$RTE_SDK/tools/dpdk_nic_bind.py --bind=uio_pci_generic $2 
$RTE_SDK/tools/dpdk_nic_bind.py --status
