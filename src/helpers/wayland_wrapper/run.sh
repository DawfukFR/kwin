systemd-run --user --wait -P -p Restart=on-failure -p FileDescriptorStoreMax=5 kwin_wayland $@
