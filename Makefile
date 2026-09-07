BUILD = build
KERNEL = kernel/linux-7.2.1/arch/x86/boot/bzImage
INITRD = build/initramfs.cpio.gz

all: $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/init

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	nasm -f bin boot/boot.asm -o $(BUILD)/boot.bin

$(BUILD)/stage2.bin: boot/stage2.asm | $(BUILD)
	nasm -f bin boot/stage2.asm -o $(BUILD)/stage2.bin

$(BUILD)/init: userland/init.asm | $(BUILD)
	nasm -f elf32 userland/init.asm -o $(BUILD)/init.o
	ld -m elf_i386 $(BUILD)/init.o -o $(BUILD)/init

# initramfs a partir do rootfs/ (cpio newc + gzip, formato que o kernel
# desempacota no boot). Sem pré-requisitos: só é refeito quando o arquivo
# não existe — pra forçar, `rm -f build/initramfs.cpio.gz` (o disco
# depende dele, então `make build/disk.img` reconstrói em cascata).
$(INITRD): | $(BUILD)
	cd rootfs && find . -print0 | cpio --null -o -H newc --owner=0:0 | gzip -9 > ../$(INITRD).tmp
	mv $(INITRD).tmp $(INITRD)

$(BUILD)/disk.img: $(BUILD)/boot.bin $(BUILD)/stage2.bin $(INITRD)
	chmod +x userland/build-disk.sh
	./userland/build-disk.sh $(BUILD)/boot.bin $(BUILD)/stage2.bin $(KERNEL) $(INITRD) $(BUILD)/disk.img

run: $(BUILD)/disk.img
	qemu-system-i386 -hda $(BUILD)/disk.img -serial mon:stdio -display none

# A4: -vga std expõe o dispositivo "Bochs VGA" que o driver de kernel
# CONFIG_DRM_BOCHS sabe dirigir via KMS (/dev/dri/card0) — sem isso não
# existe placa nenhuma pro compositor abrir, mesmo com o driver certo.
run-gui: $(BUILD)/disk.img
	qemu-system-i386 -hda $(BUILD)/disk.img -vga std -display gtk -serial mon:stdio -m 512

# A4/A4.1: monta o chroot Debian trixie i386, compila wlroots mínimo
# (sem X11/GLES2/Vulkan/GBM) + swlwm + udev, e deixa os artefatos em
# ./gui-artifacts/ (ver scripts/build-gui-i386.sh). Precisa de sudo.
# Depois: userland/build-gui-rootfs.sh <rootfs> gui-artifacts
gui-artifacts:
	sudo ./scripts/build-gui-i386.sh

clean:
	rm -rf $(BUILD)

.PHONY: run run-gui clean gui-artifacts
