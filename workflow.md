# Development Workflow: Update & Test on VM

## Prerequisites
- QEMU VM is built and the disk images exist (`jammy-server-cloudimg-amd64.img`, `seed.img`, `nvme_drive.img`)
- SPDK is already compiled inside the VM at `~/phase1_nvme/spdk/`
- SSH key is at `~/.ssh/id_ed25519`

---

## Every Time You Want to Test

### Step 1 — Boot the VM (if not already running)
```bash
# On your host machine, from the project root:
./boot_qemu.sh
```
Leave this terminal open. The VM runs in the foreground.

---

### Step 2 — Edit Your Code
Edit `src/main.cpp` (or `src/Makefile`) on your **host** machine using VS Code as normal.

---

### Step 3 — Copy Changed Files to the VM
```bash
# Run from your host machine:
scp -P 2222 -i ~/.ssh/id_ed25519 \
  /home/ofek-yankis/embedded_agency/phase1_nvme/src/main.cpp \
  ubuntu@localhost:~/phase1_nvme/src/main.cpp
```

If you also changed the Makefile:
```bash
scp -P 2222 -i ~/.ssh/id_ed25519 \
  /home/ofek-yankis/embedded_agency/phase1_nvme/src/Makefile \
  ubuntu@localhost:~/phase1_nvme/src/Makefile
```

---

### Step 4 — SSH into the VM
```bash
ssh -p 2222 -i ~/.ssh/id_ed25519 ubuntu@localhost
```

---

### Step 5 — Build the Driver (inside the VM)
```bash
cd ~/phase1_nvme/src
make clean && make
```

A successful build ends with two `g++` lines and **no errors**.

---

### Step 6 — Kernel Bypass Setup (once per VM boot)
> Only needed after a reboot. If the VM is still running from a previous session, skip this step.

```bash
sudo modprobe uio_pci_generic
sudo HUGEMEM=2048 ~/phase1_nvme/spdk/scripts/setup.sh
```

Expected output confirms the NVMe is bound:
```
0000:00:04.0 (1b36 0010): Already using the uio_pci_generic driver
INFO: Requested 1024 hugepages but 1024 already allocated
```

---

### Step 7 — Run the Driver (inside the VM)
```bash
sudo LD_LIBRARY_PATH=/home/ubuntu/phase1_nvme/spdk/dpdk/build/lib:/home/ubuntu/phase1_nvme/spdk/build/lib \
  ./b2b_nvme_driver
```

Expected clean output:
```
Booting user space NVMe arch...
Phy memory locked, scanning PCIe bus for proxy tunnels...
Probing PCIe device at address: 0000:00:04.0
SUCCESS: Exclusive hardware control established at 0000:00:04.0
Hardware bridge verified, detaching and cleaning up...
```

---

## Quick Reference (TL;DR)

| What changed | What to do |
|---|---|
| Only `main.cpp` | Steps 3 → 4 → 5 → 7 |
| Only `Makefile` | Steps 3 → 4 → 5 → 7 |
| VM was rebooted | Steps 4 → 6 → 7 |
| First time after host reboot | Steps 1 → 3 → 4 → 5 → 6 → 7 |

---

## Troubleshooting

**`librte_eal.so.26: cannot open shared object file`**
→ You forgot the `LD_LIBRARY_PATH` prefix in Step 7.

**`NVMe trtype 256 (PCIE) not available`**
→ The binary was built without `--whole-archive`. Rebuild from scratch:
`make clean && make`

**`Device 0000:00:04.0 is still attached at shutdown`**
→ Old binary without proper detach. Pull the latest `main.cpp` from host and rebuild.

**`0000:00:04.0: Active devices ... so not binding PCI dev`**
→ The NVMe is still bound to the kernel driver. Run Step 6 again.
