#!/bin/sh
set -e

ROOTFS="$1"
BASH_BIN="$2"
BUSYBOX_BIN="$3"
INIT_BIN="$4"

if [ -z "$ROOTFS" ] || [ -z "$BASH_BIN" ] || [ -z "$BUSYBOX_BIN" ] || [ -z "$INIT_BIN" ]; then
    echo "uso: $0 <destino_rootfs> <bash_bin> <busybox_bin> <init_bin>"
    exit 1
fi

mkdir -p "$ROOTFS/bin" "$ROOTFS/sbin" "$ROOTFS/etc" "$ROOTFS/dev" \
         "$ROOTFS/proc" "$ROOTFS/sys" "$ROOTFS/tmp" "$ROOTFS/var" \
         "$ROOTFS/root" "$ROOTFS/home"

cp "$BASH_BIN" "$ROOTFS/bin/bash"
chmod 755 "$ROOTFS/bin/bash"
ln -sf bash "$ROOTFS/bin/sh"

cp "$BUSYBOX_BIN" "$ROOTFS/bin/busybox"
chmod 755 "$ROOTFS/bin/busybox"

for applet in $("$BUSYBOX_BIN" --list); do
    if [ "$applet" != "sh" ] && [ "$applet" != "bash" ] && [ "$applet" != "busybox" ]; then
        ln -sf busybox "$ROOTFS/bin/$applet"
    fi
done

cp "$INIT_BIN" "$ROOTFS/sbin/init"
chmod 755 "$ROOTFS/sbin/init"
ln -sf sbin/init "$ROOTFS/init"

mknod -m 622 "$ROOTFS/dev/console" c 5 1 2>/dev/null || true
mknod -m 666 "$ROOTFS/dev/null" c 1 3 2>/dev/null || true
mknod -m 666 "$ROOTFS/dev/tty" c 5 0 2>/dev/null || true

cat > "$ROOTFS/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
EOF

cat > "$ROOTFS/etc/group" << 'EOF'
root:x:0:
EOF

echo "swl-os" > "$ROOTFS/etc/hostname"

cat > "$ROOTFS/etc/fstab" << 'EOF'
proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
EOF

echo "rootfs montado em $ROOTFS"
