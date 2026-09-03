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

$(BUILD)/disk.img: $(BUILD)/boot.bin $(BUILD)/stage2.bin
	chmod +x userland/build-disk.sh
	./userland/build-disk.sh $(BUILD)/boot.bin $(BUILD)/stage2.bin $(KERNEL) $(INITRD) $(BUILD)/disk.img

run: $(BUILD)/disk.img
	qemu-system-i386 -hda $(BUILD)/disk.img -serial mon:stdio -display none

clean:
	rm -rf $(BUILD)
