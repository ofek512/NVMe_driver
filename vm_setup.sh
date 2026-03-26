#!/bin/bash
# Run this inside the VM after every boot, before running b2b_nvme_driver.
# This replaces the manual spdk/scripts/setup.sh workflow.

set -e

echo "[1/4] Loading uio_pci_generic kernel module..."
sudo modprobe uio_pci_generic

echo "[2/4] Allocating 1024 hugepages (2GB)..."
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages > /dev/null

echo "[3/4] Mounting hugetlbfs..."
sudo mkdir -p /dev/hugepages
sudo mount -t hugetlbfs nodev /dev/hugepages 2>/dev/null || true

echo "[4/4] Binding NVMe 0000:00:04.0 to uio_pci_generic..."
# Unbind from kernel nvme driver if currently bound
if [ -e /sys/bus/pci/devices/0000:00:04.0/driver ]; then
    echo "0000:00:04.0" | sudo tee /sys/bus/pci/devices/0000:00:04.0/driver/unbind > /dev/null
fi

# Bind to uio_pci_generic (1b36:0010 is QEMU NVMe vendor:device ID)
echo "1b36 0010" | sudo tee /sys/bus/pci/drivers/uio_pci_generic/new_id > /dev/null 2>&1 || true

# Verify
DRIVER=$(readlink /sys/bus/pci/devices/0000:00:04.0/driver 2>/dev/null | xargs basename 2>/dev/null || echo "none")
if [ "$DRIVER" == "uio_pci_generic" ]; then
    echo ""
    echo "Setup complete. NVMe is bound to uio_pci_generic."
    echo "Run: cd ~/phase1_nvme/src && sudo LD_LIBRARY_PATH=/home/ubuntu/phase1_nvme/spdk/dpdk/build/lib:/home/ubuntu/phase1_nvme/spdk/build/lib ./b2b_nvme_driver"
else
    echo ""
    echo "ERROR: NVMe is still bound to: $DRIVER"
    echo "Current driver symlink: $(readlink /sys/bus/pci/devices/0000:00:04.0/driver 2>/dev/null || echo 'none')"
    exit 1
fi
