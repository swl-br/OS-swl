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
# /bin/sh aponta pro busybox (i386 estático), NÃO pro bash: o bash copiado
# acima vem do host de build (x86_64) e é inexequível no kernel i386 do
# SWL OS — com sh->bash, todo execve de script (start-gui.sh) e todo
# execl("/bin/sh") do swlwm falham com ENOEXEC e o boot cala (ver sessão
# 2026-09-07). O ash do busybox é POSIX o bastante pro que precisamos.
ln -sf busybox "$ROOTFS/bin/sh"

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

# V1 — identidade visual do terminal (shell package)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOTFS/usr/share/swl" "$ROOTFS/root"
if [ -f "$ROOT/userland/shell/shell-rc.sh" ]; then
    cp "$ROOT/userland/shell/shell-rc.sh" "$ROOTFS/usr/share/swl/shell-rc.sh"
    chmod 644 "$ROOTFS/usr/share/swl/shell-rc.sh"
fi
if [ -f "$ROOT/userland/shell/etc-profile" ]; then
    cp "$ROOT/userland/shell/etc-profile" "$ROOTFS/etc/profile"
    chmod 644 "$ROOTFS/etc/profile"
fi
if [ -f "$ROOT/userland/neo-face.txt" ]; then
    cp "$ROOT/userland/neo-face.txt" "$ROOTFS/usr/share/swl/neo-face.txt"
fi
if [ -f "$ROOT/userland/swlfetch" ]; then
    cp "$ROOT/userland/swlfetch" "$ROOTFS/bin/swlfetch"
    chmod 755 "$ROOTFS/bin/swlfetch"
fi
# ash interativo não-login: ENV aponta pro rc
# (também exportado em /etc/profile)

echo "rootfs montado em $ROOTFS"
