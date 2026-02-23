#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if [ ! -d "${OUTDIR}" ]
then
    echo "Creating directory ${OUTDIR}"
    mkdir -p ${OUTDIR}
fi
# mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here

    echo "Building the kernel"

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

fi

cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories

echo "Creating directories in the rootfs"
mkdir -p ${OUTDIR}/rootfs

if [ ! -e "${OUTDIR}/rootfs" ]
then
    echo "Failed to create rootfs directory at ${OUTDIR}/rootfs"
    exit 1
fi
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,conf,sys,tmp,usr/{lib,bin,sbin},var/log}



cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox 
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox

    echo "Configuring busybox"
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox


echo "Building busybox"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

echo "Installing busybox in rootfs"
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd "$OUTDIR"/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

program_interpreter=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | awk '{print $NF}' | tr -d '[]')
# echo "Program interpreter: ${program_interpreter}"

shared_libraries=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | awk '{print $NF}' | tr -d '[]')
# echo "Shared libraries: ${shared_libraries}"

path_to_libc=$(dirname $(dirname $(which ${CROSS_COMPILE}gcc)))

cp ${path_to_libc}/aarch64-none-linux-gnu/libc${program_interpreter} lib/

for lib in ${shared_libraries}; do
    cp ${path_to_libc}/aarch64-none-linux-gnu/libc/lib64/${lib} lib64/
done


# TODO: Make device nodes

echo "Creating device nodes"
sudo mknod -m 666 dev/null c 1 3

if [ ! -e dev/null ]; then
    echo "Failed to create null device node"
    exit 1
else
    echo "Null device node created successfully"
fi

sudo mknod -m 600 dev/console c 5 1

if [ ! -e dev/console ]; then
    echo "Failed to create console device node"
    exit 1
else 
    echo "Console device node created successfully"
fi

# TODO: Clean and build the writer utility

make -C ${FINDER_APP_DIR} clean
make -C ${FINDER_APP_DIR} CROSS_COMPILE=${CROSS_COMPILE} ARCH=${ARCH}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

cd "$OUTDIR"/rootfs 
cp ${FINDER_APP_DIR}/finder-test.sh home/
cp ${FINDER_APP_DIR}/finder.sh home/
cp ${FINDER_APP_DIR}/writer home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh home/
mkdir -p home/conf
cp ${FINDER_APP_DIR}/conf/assignment.txt home/conf/
cp ${FINDER_APP_DIR}/conf/username.txt home/conf/

# TODO: Chown the root directory

sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz

cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio

if [ ! -e "${OUTDIR}/initramfs.cpio.gz" ]; then
    echo "Failed to create initramfs.cpio.gz"
    exit 1
else
    echo "Initramfs created successfully at ${OUTDIR}/initramfs.cpio.gz"
fi

chmod 644 ${OUTDIR}/initramfs.cpio.gz  
