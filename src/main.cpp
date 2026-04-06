using namespace std;

// SPDK is in C, not C++ this prevents compiler from ruining the functions
extern "C" {
#include "spdk/env.h"
#include "spdk/nvme.h"
}
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>


struct NvmeDevice {
    struct spdk_nvme_ctrlr *ctrlr;
    struct spdk_nvme_ns    *ns;
    std::vector<struct spdk_nvme_qpair *> qpair;    // Mission B: I/O queue pair
    void                   *dma_buf;  // Mission C: DMA-safe buffer
    uint32_t                sector_size;
    uint64_t                total_sectors;
};

static std::vector<NvmeDevice> g_devices;

//Find the NVMe on PCIe bus with callback function
bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *opts){
    cout << "Probing PCIe device at address: " << trid->traddr << endl;
    return true; // Return true to attach to this device
}

// Callback fires after claiming the device — store controller for cleanup
void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts) {
    std::cout << "SUCCESS: Exclusive hardware control established at " << trid->traddr << std::endl;
    NvmeDevice dev = {};
    dev.ctrlr = ctrlr;
    g_devices.push_back(dev);
}

//Callback for write
void write_cb(void *arg, const struct spdk_nvme_cpl *completion) {
    if (spdk_nvme_cpl_is_error(completion)) {
        cerr << "ERROR: Write I/O failed with status code " << completion->status.sc << endl;
    } else {
        cout << "Write I/O completed successfully" << endl;
    }

    //cast arg to bool and set as true
    bool *write_polling = (bool *)arg;
    *write_polling = true;
}

//Callback for read
void read_cb(void *arg, const struct spdk_nvme_cpl *completion) {
    if (spdk_nvme_cpl_is_error(completion)) {
        cerr << "ERROR: Read I/O failed with status code " << completion->status.sc << endl;
    } else {
        cout << "Read I/O completed successfully" << endl;
    }

    //cast arg to bool and set as true
    bool *read_polling = (bool *)arg;
    *read_polling = true;
}

int main() {
    cout <<  "Booting user space NVMe arch..." << endl;

    // config DPDK env options — opts_size must be set before spdk_env_opts_init
    struct spdk_env_opts opts;
    opts.opts_size = sizeof(opts);
    spdk_env_opts_init(&opts);
    opts.name = "nvme_user";

    // Initialize the SPDK environment (consume 2GB of hugepages)
    if (spdk_env_init(&opts) < 0) {
        cerr << "ERROR: Unable to initialize SPDK env" << endl;
        return -1;
    }

    cout << "Phy memory locked, scanning PCIe bus for proxy tunnels..." << endl;

    // Scan the PCIe bus for NVMe devices and claim them with the probe callback
    if (spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL) != 0) {
        cerr << "FATAL: NVMe probe failed" << endl;
        spdk_env_fini();
        return -1;
    }

    // Call ctrlr_get_data() to see the model and serial and get namespace 1 with nvme_ctrlr_get_ns() to verify we have access to the device
    for (auto &dev : g_devices) {
        const struct spdk_nvme_ctrlr_data *data;
        data = spdk_nvme_ctrlr_get_data(dev.ctrlr);
        cout << "Model number: " << string((char *)data->mn, sizeof(data->mn))
             << ", Serial number: " << string((char *)data->sn, sizeof(data->sn)) << endl;

        dev.ns = spdk_nvme_ctrlr_get_ns(dev.ctrlr, 1);
        if (dev.ns == nullptr || !spdk_nvme_ns_is_active(dev.ns)) {
            cerr << "ERROR: Namespace 1 not active on controller" << endl;
            spdk_nvme_detach(dev.ctrlr);
            spdk_env_fini();
            return -1;
        }

        cout << "Allocating I/O queue pairs for NVMe operations..." << endl;
        uint32_t index;
        struct spdk_nvme_io_qpair_opts qpairopts;
        dev.qpair.resize(spdk_env_get_last_core() + 1, nullptr);
        SPDK_ENV_FOREACH_CORE(index) {
            spdk_nvme_ctrlr_get_default_io_qpair_opts(dev.ctrlr, &qpairopts, sizeof(qpairopts));
            qpairopts.io_queue_size = 128; // Set queue depth to 128
            qpairopts.delay_cmd_submit = true; // Enable delayed command submission for better batching
            //store in vector indexed by core ID
            dev.qpair[index] = spdk_nvme_ctrlr_alloc_io_qpair(dev.ctrlr, &qpairopts, sizeof(qpairopts));
            if (dev.qpair[index] == nullptr) {
                cerr << "ERROR: Failed to allocate I/O queue pair for core " << index << endl;
                spdk_nvme_detach(dev.ctrlr);
                spdk_env_fini();
                return -1;
            }
        }        

        // dev.qpair = spdk_nvme_ctrlr_alloc_io_qpair(dev.ctrlr, nullptr, 0); // Mission B: Allocate I/O queue pair
        // if (dev.qpair == nullptr) {
        //     cerr << "ERROR: Failed to allocate I/O queue pair" << endl;
        //     spdk_nvme_detach(dev.ctrlr);
        //     spdk_env_fini();
        //     return -1;
    }

    //     dev.sector_size = spdk_nvme_ns_get_sector_size(dev.ns);
    //     dev.total_sectors = spdk_nvme_ns_get_num_sectors(dev.ns);
    //     cout << "Allocating DMA-safe buffer for I/O operations..." << endl;
    //     dev.dma_buf = spdk_dma_zmalloc(dev.sector_size, dev.sector_size, NULL); // Mission C: Allocate DMA-safe buffer
    //     if (dev.dma_buf == nullptr) {
    //         cerr << "ERROR: Failed to allocate DMA-safe buffer" << endl;
    //         spdk_nvme_ctrlr_free_io_qpair(dev.qpair);
    //         spdk_nvme_detach(dev.ctrlr);
    //         spdk_env_fini();
    //         return -1;
    //     }

    //     cout << "Bytes per sector: " << dev.sector_size << endl;
    //     cout << "Total sectors: " << dev.total_sectors << endl;
    //     cout << "Capacity: " << (uint64_t)dev.sector_size * dev.total_sectors / (1024 * 1024) << " MB" << endl;
    // }

    // //Writing to dma_buffer for the first NVMe device as a show of proof
    // const char *test = "Hello NVMe";
    // memcpy(g_devices[0].dma_buf, test, strlen(test));

    // bool write_polling = false;
    // cout << "Submitting write I/O to NVMe device..." << endl;
    // spdk_nvme_ns_cmd_write(g_devices[0].ns, g_devices[0].qpair, g_devices[0].dma_buf, 0, 1, write_cb, &write_polling, 0); // Mission D: Submit write I/O
    // // Poll for completion of the write I/O
    // while (!write_polling) {
    //     spdk_nvme_qpair_process_completions(g_devices[0].qpair, 0);
    // }

    // //Reading the first sector back
    // // Clear the DMA buffer
    // memset(g_devices[0].dma_buf, 0, g_devices[0].sector_size);

    // //Submit read command
    // bool read_polling = false;
    // spdk_nvme_ns_cmd_read(g_devices[0].ns, g_devices[0].qpair, g_devices[0].dma_buf, 0, 1, read_cb, &read_polling, 0); // Mission D: Submit read I/O
    // // Poll for completion of the read I/O
    // while (!read_polling) {
    //     spdk_nvme_qpair_process_completions(g_devices[0].qpair, 0);
    // }

    // //Data is now in the buffer, we can print it
    // cout << "Data read from NVMe: " << (char *)g_devices[0].dma_buf << endl;

    // cout << "Finished show of proof. Detaching and cleaning up..." << endl;

    // // Detach all controllers before freeing the environment
    // for (auto &dev : g_devices) {
    //     spdk_dma_free(dev.dma_buf); // Mission C: Free DMA-safe buffer
    //     spdk_nvme_ctrlr_free_io_qpair(dev.qpair); // Mission B: Free I/O queue pair
    //     spdk_nvme_detach(dev.ctrlr);
    // }

    spdk_env_fini();

    return 0;
}