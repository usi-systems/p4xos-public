#! /bin/bash

# TEXT COLOR
GREEN='\033[1;32m'
RED='\033[1;31m'
NC='\033[0m' # No Color

echo "###############################################"
echo "###          ${RED}P4ELTE INSTALL SCRIPT${NC}          ###"
echo "###         author: ${GREEN}Pietro Bressana${NC}         ###"
echo "###            please run as ${RED}ROOT${NC}           ###"
echo "###############################################"

# SET LOCAL VARIABLES
echo "${GREEN}[STATUS]: setting local variables...${NC}"

BASE_DIR="/paxos-dpdk"

HUGE_DIR="/mnt/huge_1GB"

MOD_DIR="/lib/modules/3.19.0-58-generic/kernel/drivers/igb_uio"

# network interface: eth18
PCI_PATH_A="0000:01:00.0"

# network interface: eth21
PCI_PATH_B="0000:01:00.1"

LOG_DIR="/home/netfpga/dpdk_conf.log"

DPDK_VERSION="2.2.0"

DPDK_INSTALLDIR="${BASE_DIR}/dpdk-${DPDK_VERSION}"
DPDK_DWLDIR="${DPDK_INSTALLDIR}/download"
DPDK_SRCDIR="${DPDK_INSTALLDIR}/src"
DPDK_FILE="dpdk-${DPDK_VERSION}"
DPDK_REPO="http://fast.dpdk.org/rel/${DPDK_FILE}.tar.xz"

P4ELTE_INSTALLDIR="${BASE_DIR}/p4elte"
P4ELTE_DWLDIR="${P4ELTE_INSTALLDIR}/download"
P4ELTE_SRCDIR="${P4ELTE_INSTALLDIR}/src"
P4ELTE_FILE="master"
P4ELTE_REPO="https://github.com/P4ELTE/p4c/archive/${P4ELTE_FILE}.zip"

P4HLIR_INSTALLDIR="${BASE_DIR}/p4hlir"
P4HLIR_DWLDIR="${P4HLIR_INSTALLDIR}/download"
P4HLIR_SRCDIR="${P4HLIR_INSTALLDIR}/src"
P4HLIR_FILE="master"
P4HLIR_REPO="https://github.com/p4lang/p4-hlir/archive/${P4HLIR_FILE}.zip"

RTE_TARGET="x86_64-native-linuxapp-gcc"
RTE_SDK="${DPDK_SRCDIR}/dpdk-2.2.0"

# SET GLOBAL ENVIRONMENT VARIABLES
echo "${GREEN}[STATUS]: setting global environment variables...${NC}"

cd /etc/

echo "RTE_TARGET=\"x86_64-native-linuxapp-gcc\"" >> environment
echo "RTE_SDK=\"${DPDK_SRCDIR}/dpdk-2.2.0\"" >> environment
echo "DPDK_DIR=\"${DPDK_SRCDIR}/dpdk-2.2.0\"" >> environment
echo "P4HLIR_DIR=\"${P4HLIR_SRCDIR}/p4-hlir-master\"" >> environment
echo "P4ELTE_DIR=\"${P4ELTE_SRCDIR}/p4c-master\"" >> environment
echo "HUGE_DIR=\"${HUGE_DIR}\"" >> environment

# GENERATE FOLDERS
echo "${GREEN}[STATUS]: generating folders...${NC}"

mkdir -pv ${DPDK_DWLDIR}
mkdir -pv ${DPDK_SRCDIR}

mkdir -pv ${P4ELTE_DWLDIR}
mkdir -pv ${P4ELTE_SRCDIR}

mkdir -pv ${P4HLIR_DWLDIR}
mkdir -pv ${P4HLIR_SRCDIR}

mkdir -pv ${HUGE_DIR}

mkdir -pv ${MOD_DIR}

# UPDATE & UPGRADE
echo "${GREEN}[STATUS]: updating & upgrading...${NC}"

apt-get -y update
apt-get -y upgrade

# INSTALL DEPENDENCIES
echo "${GREEN}[STATUS]: installing dependencies...${NC}"

apt-get -y install unzip
apt-get -y install linux-headers-$(uname -r)
apt-get -y install libpcap-dev
apt-get -y install python-yaml
apt-get -y install graphviz
apt-get -y install python-ply
apt-get -y install python-setuptools
apt-get -y install gcc-multilib

# DOWNLOAD SETUP FILES
echo "${GREEN}[STATUS]: downloading setup files...${NC}"

cd ${DPDK_DWLDIR}
if [ ! -f ${DPDK_FILE}.tar.xz ]; then
wget ${DPDK_REPO}
fi

cd ${P4HLIR_DWLDIR}
if [ ! -f ${P4HLIR_FILE}.zip ]; then
wget ${P4HLIR_REPO}
fi

cd ${P4ELTE_DWLDIR}
if [ ! -f ${P4ELTE_FILE}.zip ]; then
wget ${P4ELTE_REPO}
fi

# EXTRACT SETUP FILES
echo "${GREEN}[STATUS]: extracting setup files...${NC}"

cd ${DPDK_DWLDIR}
tar --overwrite -xf ${DPDK_FILE}.tar.xz -C ${DPDK_SRCDIR}

cd ${P4HLIR_DWLDIR}
unzip -o ${P4HLIR_FILE}.zip -d ${P4HLIR_SRCDIR}

cd ${P4ELTE_DWLDIR}
unzip -o ${P4ELTE_FILE}.zip -d ${P4ELTE_SRCDIR}

# *********************************************
# ***             INSTALL DPDK              ***
# *********************************************
echo "${GREEN}[STATUS]: installing DPDK...${NC}"

cd ${DPDK_SRCDIR}/${DPDK_FILE}

make config T=x86_64-native-linuxapp-gcc
make install T=x86_64-native-linuxapp-gcc
sed -ri 's,(PMD_PCAP=).*,\1y,' build/.config
make

# *********************************************
# ***             INSTALL P4HLIR            ***
# *********************************************
echo "${GREEN}[STATUS]: installing P4HLIR...${NC}"

cd ${P4HLIR_SRCDIR}/p4-hlir-master
python setup.py install
# *********************************************

# *********************************************
# ***             CONFIGURE GRUB            ***
# *********************************************
echo "${GREEN}[STATUS]: configuring GRUB...${NC}"

cd /etc/default/
cp grub grub_stbl
sed -i 's/splash/splash default_hugepagesz=1G hugepagesz=1G hugepages=4 isolcpus=4,5/' grub
update-grub
# *********************************************

# *********************************************
# ***      CONFIGURE DRIVER MODULES         ***
# *********************************************
echo "${GREEN}[STATUS]: configuring DRIVER MODULES...${NC}"

cd /etc/
cp modules modules_stbl
echo "# DPDK DRIVER MODULES" >> modules
echo "uio" >> modules
echo "igb_uio" >> modules

cp ${RTE_SDK}/${RTE_TARGET}/kmod/igb_uio.ko ${MOD_DIR}

depmod
# *********************************************

# *********************************************
# ***        CONFIGURE AUTOMATIC SCRIPT     ***
# *********************************************
echo "${GREEN}[STATUS]: configuring AUTOMATIC SCRIPT...${NC}"

cd /etc/init.d

# CREATE AUTOMATIC SCRIPT
cat >dpdk_conf <<EOF
#! /bin/bash

echo "" >> ${LOG_DIR}
echo "*******************************************" >> ${LOG_DIR}
echo | date >> ${LOG_DIR}
echo "*******************************************" >> ${LOG_DIR}

##########################
###   MOUNT HUGETLBFS
##########################

echo "mounting hugetlbfs..." >> ${LOG_DIR}
mount -t hugetlbfs nodev ${HUGE_DIR}

##########################
### BIND NICS TO MODULE
##########################

# igb_uio interfaces:
ifconfig eth18 down
ifconfig eth21 down

# ixgbe interfaces:
ifconfig eth20 up
ifconfig eth22 up

# binding PCI_PATH_A
echo "binding ${PCI_PATH_A} nic to driver module..." >> ${LOG_DIR}
if  /sbin/lsmod  | grep -q igb_uio ; then
	${RTE_SDK}/tools/dpdk_nic_bind.py -b igb_uio ${PCI_PATH_A}
	echo "${PCI_PATH_A} OK" >> ${LOG_DIR}
else
	echo "# ${PCI_PATH_A} : Please load the 'igb_uio' kernel module before querying or " >> ${LOG_DIR}
	echo "# adjusting NIC device bindings" >> ${LOG_DIR}
fi

# binding PCI_PATH_B
echo "binding ${PCI_PATH_B} nic to driver module..." >> ${LOG_DIR}
if  /sbin/lsmod  | grep -q igb_uio ; then
	${RTE_SDK}/tools/dpdk_nic_bind.py -b igb_uio ${PCI_PATH_B}
	echo "${PCI_PATH_B} OK" >> ${LOG_DIR}
else
	echo "# ${PCI_PATH_B} : Please load the 'igb_uio' kernel module before querying or " >> ${LOG_DIR}
	echo "# adjusting NIC device bindings" >> ${LOG_DIR}
fi
EOF

chmod +x dpdk_conf

update-rc.d dpdk_conf start 99 2 .
# *********************************************

# REBOOT
echo "${GREEN}[STATUS]: setup complete!!!${NC}"
echo "${RED}[STATUS]: Please REBOOT the system!!!${NC}"
