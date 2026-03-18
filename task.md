# Systems Architecture: User-Space NVMe Driver (C++)

## Overview
This document outlines the required research and architectural understanding necessary to engineer a user-space NVMe driver using the SPDK framework. The objective is to bypass the standard Linux kernel NVMe driver, take direct control of the hardware via the PCIe bus, and issue asynchronous I/O commands using custom C++ object wrappers.

---

## Module 1: The Build & Interoperability Architecture
*SPDK and DPDK are written in strictly procedural C. Your application will be object-oriented C++. You must bridge the compilation gap.*

### 1.1 Name Mangling and Linkage
* **Task:** Understand how the C++ compiler (`g++`) alters function names during compilation (name mangling) to support function overloading, and why this breaks C libraries.
* **Deliverable:** Learn how to implement `extern "C"` blocks around SPDK header includes to force the C++ compiler to use standard C linkage for those specific functions.

### 1.2 Static vs. Dynamic Linking (The DPDK Problem)
* **Task:** DPDK is composed of hundreds of individual static archives (`.a`). If linked incorrectly, the linker will strip out "unused" hardware drivers, causing the application to fail at runtime when it tries to find the NVMe device.
* **Deliverable:** Understand the `--whole-archive` and `--no-whole-archive` linker flags in GCC.
* **Deliverable:** Research `pkg-config`. Understand how to extract the exact required linker flags directly from the `spdk.pc` file rather than hardcoding them in the `Makefile`.

---

## Module 2: Memory Architecture & Direct Memory Access (DMA)
*Standard `malloc()` or `new` allocates virtual memory. Handing a virtual address to a physical PCIe device results in memory corruption and kernel panics.*

### 2.1 Virtual vs. Physical Addressing
* **Task:** Review how the Linux Memory Management Unit (MMU) translates virtual addresses to physical addresses using page tables.
* **Task:** Understand why user-space applications generally do not know their physical RAM addresses.

### 2.2 Direct Memory Access (DMA)
* **Task:** Understand how DMA allows the NVMe controller to read and write directly to physical RAM modules, completely bypassing the CPU.
* **Task:** Research IOMMU (Input-Output Memory Management Unit) and how it protects memory during DMA. Understand why our current setup (without an IOMMU) requires the less secure `uio_pci_generic` driver.

### 2.3 Hugepages & Memory Pinning
* **Task:** Standard Linux memory pages are 4KB and can be swapped to disk. If the OS swaps a page while the NVMe drive is writing to it via DMA, the system crashes.
* **Deliverable:** Understand how allocating 2MB or 1GB Hugepages permanently pins the memory in physical RAM, making it safe for hardware DMA.
* **Deliverable:** Learn the DPDK Environment Abstraction Layer (EAL) memory functions (e.g., `spdk_dma_zmalloc` and `spdk_dma_free`) which must replace `new` and `delete` for any data buffer sent to the drive.

---

## Module 3: PCIe Bus & Memory-Mapped I/O (MMIO)
*You will not use `read()` or `write()` system calls. You will manipulate physical motherboard registers.*

### 3.1 BDF Addressing
* **Task:** Learn how Linux identifies PCIe devices using the Bus, Device, Function (BDF) format (e.g., `0000:00:04.0`).

### 3.2 Base Address Registers (BARs)
* **Task:** Research PCIe BARs. Every PCIe device has memory registers built into the silicon. For NVMe controllers, BAR0 contains the exact registers used to control the drive.
* **Deliverable:** Understand that the `uio_pci_generic` driver creates a tunnel that exposes BAR0 to the user-space application.

### 3.3 Memory-Mapped I/O
* **Task:** Understand how the `mmap()` system call takes the physical PCIe registers exposed by the UIO driver and maps them into your C++ application's virtual memory space. You flip bits in a virtual C++ pointer, and the OS instantly flips the physical bits on the motherboard.

---

## Module 4: NVMe Protocol Specification
*NVMe is an asynchronous, queue-based protocol over PCIe. It does not understand files or directories; it only understands Logical Block Addresses (LBAs).*

### 4.1 Queue Pairs (Submission & Completion)
* **Task:** Understand the circular ring-buffer architecture.
    * **Submission Queue (SQ):** Where the host (your C++ code) places commands (Read, Write, Identify).
    * **Completion Queue (CQ):** Where the controller (the NVMe hardware) posts the status of completed commands.
* **Deliverable:** Distinguish between the **Admin Queue Pair** (used for initializing the drive and creating other queues) and **I/O Queue Pairs** (used for actual data transfer).

### 4.2 The Doorbell Registers
* **Task:** Placing a command in the SQ in RAM does nothing on its own. The hardware must be notified.
* **Deliverable:** Research NVMe Doorbells. To execute a command, your code must write the new "Tail" index of the Submission Queue directly into a specific MMIO register on the NVMe controller. This "rings the doorbell" and wakes the hardware up.

### 4.3 Data Structures: PRPs and SGLs
* **Task:** When you tell the drive to read data, you must tell it exactly which physical RAM addresses to put the data into.
* **Deliverable:** Understand Physical Region Page (PRP) lists and Scatter Gather Lists (SGLs), which are the data structures NVMe uses to handle data buffers that span multiple memory pages.

---

## Module 5: SPDK API Mapping
*SPDK provides an abstraction layer over the raw MMIO and queue mechanics. You must understand these specific functions to build your C++ classes.*

### 5.1 Environment Initialization
* **Target API:** `spdk_env_opts_init()` and `spdk_env_init()`.
* **Purpose:** Consumes the allocated Hugepages and initializes the DPDK core masking.

### 5.2 Device Probing and Attachment
* **Target API:** `spdk_nvme_probe()`.
* **Purpose:** Scans the PCIe bus.
* **Deliverable:** Understand how the asynchronous callback system works in SPDK. You must write a `probe_cb` (to decide if you want to take control of a specific BDF) and an `attach_cb` (which gives you the `spdk_nvme_ctrlr` struct representing the physical hardware).

### 5.3 I/O Queue Allocation
* **Target API:** `spdk_nvme_ctrlr_alloc_io_qpair()`.
* **Purpose:** Creates the actual Submission and Completion queues in DMA-safe memory.

### 5.4 Asynchronous Polling
* **Target API:** `spdk_nvme_qpair_process_completions()`.
* **Purpose:** NVMe does not use hardware interrupts in user-space. Your C++ code must actively poll (check) the Completion Queue in a loop to see if the hardware has finished reading or writing.
