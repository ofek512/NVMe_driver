# User-Space NVMe Driver

A C++ application that bypasses the Linux kernel to control an NVMe drive directly from user space over PCIe. Built on top of SPDK and DPDK.

## What This Does

```
Standard Linux I/O:     App → syscall → kernel driver → NVMe
This project:           App → PCIe BAR0 registers → NVMe
```

The program:
1. Locks 2GB of hugepages in physical RAM for DMA-safe memory
2. Scans the PCIe bus and claims the NVMe controller (bypassing the kernel driver)
3. Reads the device identity (model, serial, capacity)
4. Allocates I/O Submission and Completion Queues in hugepage memory
5. Writes "Hello NVMe" to LBA 0 via DMA
6. Reads it back and prints it — proving a full hardware round-trip
7. Cleans up: frees buffers, destroys queues, detaches from hardware

No kernel I/O syscalls are used. The NVMe controller reads and writes directly to physical RAM via DMA.

## Project Structure

```
├── boot_qemu.sh          # Launches the QEMU VM with a virtual NVMe drive
├── user-data.yaml         # Cloud-init config (SSH key, user "ubuntu")
├── src/
│   ├── main.cpp           # The driver
│   └── Makefile           # Builds against SPDK/DPDK static libraries
└── spdk/                  # SPDK framework (git submodule, gitignored)
```

## Prerequisites

- **Host machine:** Ubuntu with QEMU installed
- **VM disk images:** `jammy-server-cloudimg-amd64.img`, `seed.img`, `nvme_drive.img`
- **SSH key:** `~/.ssh/id_ed25519` (must match the key in `user-data.yaml`)
- **SPDK built inside the VM** at `~/phase1_nvme/spdk/`

## How to Run

### 1. Boot the VM

```bash
./boot_qemu.sh
```

Leave this terminal open. The VM runs in the foreground.

### 2. SSH into the VM (new terminal)

```bash
ssh -p 2222 -i ~/.ssh/id_ed25519 ubuntu@localhost
```

### 3. Kernel bypass setup (once per VM boot)

Run the setup script from inside the VM:

```bash
~/phase1_nvme/vm_setup.sh
```

Expected output:
```
[1/4] Loading uio_pci_generic kernel module...
[2/4] Allocating 1024 hugepages (2GB)...
[3/4] Mounting hugetlbfs...
[4/4] Binding NVMe 0000:00:04.0 to uio_pci_generic...

Setup complete. NVMe is bound to uio_pci_generic.
```

### 4. Build the driver

```bash
cd ~/phase1_nvme/src
make clean && make
```

### 5. Run

```bash
sudo LD_LIBRARY_PATH=/home/ubuntu/phase1_nvme/spdk/dpdk/build/lib:/home/ubuntu/phase1_nvme/spdk/build/lib \
  ./b2b_nvme_driver
```

### Expected output

```
Booting user space NVMe arch...
Phy memory locked, scanning PCIe bus for proxy tunnels...
Probing PCIe device at address: 0000:00:04.0
SUCCESS: Exclusive hardware control established at 0000:00:04.0
Model number: QEMU NVMe Ctrl          , Serial number: deadbeef
Allocating I/O queue pair for NVMe operations...
Allocating DMA-safe buffer for I/O operations...
Bytes per sector: 512
Total sectors: ...
Capacity: ... MB
Submitting write I/O to NVMe device...
Write I/O completed successfully
Read I/O completed successfully
Data read from NVMe: Hello NVMe
Finished show of proof. Detaching and cleaning up...
```

## Development Workflow

When you edit code on the host and want to test:

```bash
# 1. Copy changed files to VM (from host)
scp -P 2222 -i ~/.ssh/id_ed25519 src/main.cpp ubuntu@localhost:~/phase1_nvme/src/
scp -P 2222 -i ~/.ssh/id_ed25519 src/Makefile ubuntu@localhost:~/phase1_nvme/src/

# 2. SSH in
ssh -p 2222 -i ~/.ssh/id_ed25519 ubuntu@localhost

# 3. First boot only: run setup
~/phase1_nvme/vm_setup.sh

# 4. Build and run
cd ~/phase1_nvme/src && make clean && make
sudo LD_LIBRARY_PATH=/home/ubuntu/phase1_nvme/spdk/dpdk/build/lib:/home/ubuntu/phase1_nvme/spdk/build/lib \
  ./b2b_nvme_driver
```

## FAQ / Troubleshooting

### `ssh: connect to host localhost port 2222: Connection refused`
The VM isn't running. Run `./boot_qemu.sh` first and wait ~60 seconds for it to boot.

### `Module uio_pci_generic not found`
The cloud image doesn't ship this module. Install it:
```bash
sudo apt-get update
sudo apt-get install -y linux-modules-extra-$(uname -r)
sudo modprobe uio_pci_generic
```

### `librte_eal.so.26: cannot open shared object file`
You forgot the `LD_LIBRARY_PATH` prefix when running the binary. Use the full `sudo LD_LIBRARY_PATH=... ./b2b_nvme_driver` command.

### `NVMe trtype 256 (PCIE) not available`
The binary was linked without `--whole-archive`. The linker stripped the PCIe transport registration code. Rebuild with `make clean && make` — the Makefile already includes the correct flags.

### `Device 0000:00:04.0 is still attached at shutdown`
Old binary without proper detach/cleanup. Pull the latest `main.cpp` and rebuild.

### `No valid drivers found [vfio-pci, uio_pci_generic, igb_uio]`
None of the PCI passthrough kernel modules are loaded. See the `Module uio_pci_generic not found` fix above.

### `EAL: No free 2048 kB hugepages reported on node 0`
Hugepages weren't allocated. Run `vm_setup.sh` again. If it keeps failing:
```bash
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages  # should show 1024
```

### `Segmentation fault` during probe
The NVMe device is still bound to the kernel driver. Run `vm_setup.sh` to rebind it.
You can verify the current binding with:
```bash
ls -la /sys/bus/pci/devices/0000:00:04.0/driver
# Should show -> .../uio_pci_generic, not .../nvme
```

### Build fails with `undefined reference to EVP_*` or `uuid_parse`
Missing system libraries. The Makefile's `SYS_LIBS` line should include `-lssl -lcrypto -luuid -lnuma -ldl -lpthread -lrt`.

### Build fails with `crc16_t10dif` / `xor_gen` undefined
Missing ISA-L library. The Makefile should end the link line with `../spdk/isa-l/.libs/libisal.a`.

### VM disk full / `update-initramfs` stuck
Grow the disk from the host:
```bash
qemu-img resize jammy-server-cloudimg-amd64.img +4G
```
Reboot the VM — cloud-init auto-expands the partition.

### SPDK build fails with `isalbuild: No such file or directory`
The SPDK submodule directories weren't fully copied to the VM. Rsync the missing `*build` directories from the host's `spdk/` folder.

## Architecture Overview

```
┌──────────────────────────────────────────────────────┐
│  main.cpp (user space)                               │
│                                                      │
│  spdk_env_init()          → lock hugepages           │
│  spdk_nvme_probe()        → scan PCIe, claim device  │
│  spdk_nvme_ctrlr_get_ns() → get namespace 1          │
│  alloc_io_qpair()         → create SQ/CQ in hugepgs  │
│  spdk_dma_zmalloc()       → allocate DMA buffer      │
│  spdk_nvme_ns_cmd_write() → submit write to SQ       │
│  process_completions()    → poll CQ for result       │
│  spdk_nvme_ns_cmd_read()  → submit read to SQ        │
│  process_completions()    → poll CQ for result       │
│                                                      │
│        ┌──────────┐    DMA     ┌──────────────────┐  │
│        │ Hugepage │   <---->   │ NVMe Controller  │  │
│        │   RAM    │            │  (PCIe BAR0)     │  │
│        └──────────┘            └──────────────────┘  │
│                                                      │
│  uio_pci_generic driver exposes BAR0 to userspace    │
│  No kernel I/O syscalls — all communication is       │
│  through MMIO registers and DMA over hugepages       │
└──────────────────────────────────────────────────────┘
```
