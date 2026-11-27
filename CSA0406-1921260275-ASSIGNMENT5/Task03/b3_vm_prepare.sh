#!/usr/bin/env bash
# B3.1: build a KVM virtual machine (Ubuntu 24.04 cloud image) that runs the same portal app with gunicorn.
set -euo pipefail
R=${R:?R is not set}
V="$R/Task03/vm"
mkdir -p "$V"
sudo apt-get install -y qemu-system-x86 qemu-utils cloud-image-utils
sudo chmod a+rw /dev/kvm
[ -f "$V/base.img" ] || curl -fL -o "$V/base.img" https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
rm -f "$V/vm.qcow2"
qemu-img create -f qcow2 -F qcow2 -b "$V/base.img" "$V/vm.qcow2" 5G
{
  cat <<'YAML'
#cloud-config
package_update: true
packages:
  - python3-flask
  - gunicorn
write_files:
  - path: /opt/portal/app.py
    content: |
YAML
  sed 's/^/      /' "$R/Task02/app.py"
  cat <<'YAML'
  - path: /etc/systemd/system/portal.service
    content: |
      [Unit]
      Description=Result Portal
      After=network.target
      [Service]
      ExecStart=/usr/bin/gunicorn --workers 1 --bind 0.0.0.0:8000 --chdir /opt/portal app:app
      Restart=always
      [Install]
      WantedBy=multi-user.target
runcmd:
  - systemctl daemon-reload
  - systemctl enable --now portal
power_state:
  mode: poweroff
  message: provisioning finished
  timeout: 30
  condition: true
YAML
} > "$V/user-data"
echo "instance-id: portal-vm-1" > "$V/meta-data"
cloud-localds "$V/seed.img" "$V/user-data" "$V/meta-data"
echo "first boot: installing packages inside the VM, it powers off by itself when done (a few minutes)..."
timeout 900 qemu-system-x86_64 -enable-kvm -cpu host -m 1024 -smp 2 \
  -drive file="$V/vm.qcow2",if=virtio -drive file="$V/seed.img",format=raw,if=virtio \
  -nic user -display none -serial file:"$V/console.log" -monitor none
echo "qemu exit code: $?"
qemu-img info "$V/vm.qcow2" | head -8
du -h "$V/base.img" "$V/vm.qcow2"
