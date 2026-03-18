using namespace std;

// SPDK is in C, not C++ this prevents compiler from ruining the functions
extern "C" {
#include "spdk/env.h"
#include "spdk/nvme.h"
}
#include <iostream>
#include <vector>

static std::vector<struct spdk_nvme_ctrlr *> g_controllers;

//Find the NVMe on PCIe bus with callback function
bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *opts){
    cout << "Probing PCIe device at address: " << trid->traddr << endl;
    return true; // Return true to attach to this device
}

// Callback fires after claiming the device — store controller for cleanup
void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts) {
    std::cout << "SUCCESS: Exclusive hardware control established at " << trid->traddr << std::endl;
    g_controllers.push_back(ctrlr);
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

    cout << "Hardware bridge verified, detaching and cleaning up..." << endl;

    // Detach all controllers before freeing the environment
    for (auto *ctrlr : g_controllers) {
        spdk_nvme_detach(ctrlr);
    }

    spdk_env_fini();

    return 0;
}