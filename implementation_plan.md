# Implementation Plan: User-Space C++ NVMe Driver

## Phase 1: The Build System (`Makefile`)
**Objective:** Successfully compile a C++ file against the static C archives of SPDK and DPDK.
* [ ] **Task 1.1:** Write a `Makefile` that uses `pkg-config` to dynamically extract the DPDK/SPDK linker flags.
* [ ] **Task 1.2:** Configure the `Makefile` to link against the required DPDK libraries (`--whole-archive`).
* [ ] **Task 1.3:** Create the C++ `main.cpp` boilerplate.
* [ ] **Task 1.4:** Wrap all SPDK `#include` directives inside an `extern "C"` block to prevent C++ name mangling.
* [ ] **Task 1.5:** Successfully run `make` and produce a binary without `undefined reference` linker errors.

## Phase 2: Environment Initialization
**Objective:** Consume the 2048MB of physical Hugepages and prepare the DPDK memory manager.
* [ ] **Task 2.1:** Create an `spdk_env_opts` struct.
* [ ] **Task 2.2:** Call `spdk_env_opts_init()` to populate the struct with default values.
* [ ] **Task 2.3:** Set the application name in the options struct (e.g., "b2b_nvme_driver").
* [ ] **Task 2.4:** Execute `spdk_env_init()`. Verify that it returns `0` (success), confirming the Hugepages are locked and ready for DMA.

## Phase 3: Hardware Probing & Attachment
**Objective:** Scan the PCIe bus, locate the NVMe controller bound to `uio_pci_generic`, and gain exclusive access to its Base Address Registers (BARs).
* [ ] **Task 3.1:** Implement the `probe_cb` (Probe Callback) function. This function must inspect the PCIe Vendor/Device ID and return `true` to claim the device.
* [ ] **Task 3.2:** Implement the `attach_cb` (Attach Callback) function. This function executes when the device is successfully claimed. It must save the provided `spdk_nvme_ctrlr` pointer to a global variable or class member so your program can use it later.
* [ ] **Task 3.3:** Call `spdk_nvme_probe()`, passing in the PCIe transport ID and your two callback functions. 
* [ ] **Task 3.4:** Add a check to ensure the `spdk_nvme_ctrlr` pointer is not null after probing finishes.

## Phase 4: Queue Allocation & DMA Setup
**Objective:** Establish the Submission and Completion queues in physical memory to handle read/write commands.
* [ ] **Task 4.1:** Call `spdk_nvme_ctrlr_alloc_io_qpair()` using your saved controller pointer. Save the returned `spdk_nvme_qpair` pointer.
* [ ] **Task 4.2:** Identify the active namespace on the NVMe drive using `spdk_nvme_ctrlr_get_ns()`. Usually, this is namespace ID `1`.
* [ ] **Task 4.3:** Allocate a DMA-safe memory buffer for data transfer using `spdk_dma_zmalloc()`. Ensure it is aligned to the block size of the NVMe namespace (typically 512 bytes or 4096 bytes).

## Phase 5: I/O Execution & Hardware Polling
**Objective:** Write data to the drive and wait for the hardware to confirm completion.
* [ ] **Task 5.1:** Write test data into your DMA-safe memory buffer.
* [ ] **Task 5.2:** Implement an I/O completion callback function. This will be triggered when the hardware finishes the write.
* [ ] **Task 5.3:** Submit a write command to the Submission Queue using `spdk_nvme_ns_cmd_write()`. You must provide the namespace, the queue pair, the DMA buffer, the starting LBA (Logical Block Address), and the completion callback.
* [ ] **Task 5.4:** Create a `while` loop that calls `spdk_nvme_qpair_process_completions()`. This loop physically checks the Completion Queue in RAM. It must run continuously until your I/O completion callback sets a boolean flag indicating the hardware is done.

## Phase 6: Graceful Teardown
**Objective:** Release the physical memory and detach from the hardware to prevent kernel panics on exit.
* [ ] **Task 6.1:** Free the DMA-safe memory buffer using `spdk_dma_free()`.
* [ ] **Task 6.2:** Free the I/O queue pair using `spdk_nvme_ctrlr_free_io_qpair()`.
* [ ] **Task 6.3:** Detach from the NVMe controller using `spdk_nvme_detach()`.
* [ ] **Task 6.4:** Clean up the DPDK environment using `spdk_env_fini()`.
