#!/bin/bash
qemu-system-x86_64 \
  -m 4G \
  -smp 4 \
  -cpu Nehalem \
  -nographic \
  -drive file=jammy-server-cloudimg-amd64.img,format=qcow2,if=virtio \
  -drive file=seed.img,format=raw,if=virtio \
  -drive file=nvme_drive.img,format=qcow2,if=none,id=nvmegen1 \
  -device nvme,serial=deadbeef,drive=nvmegen1 \
  -net nic,model=virtio \
  -net user,hostfwd=tcp::2222-:22
