AS = nasm
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
OBJCOPY = x86_64-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -O2 -Wall -Wextra -m32 -fno-pic -fno-pie -mno-sse -mno-sse2 -mno-mmx -mno-avx -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld

BOOTLOADER = bootloader.bin
KERNEL = kernel.bin
OS_IMAGE = os-image.bin

all: $(OS_IMAGE)

$(BOOTLOADER): bootloader.asm $(KERNEL)
	@size=$$(stat -f%z $(KERNEL) 2>/dev/null || stat -c%s $(KERNEL)); \
	sectors=$$(( (size + 511) / 512 )); \
	$(AS) -f bin -DKERNEL_SECTORS=$$sectors $< -o $@

kernel_entry.o: kernel_entry.asm
	$(AS) -f elf32 $< -o $@

src/proc/context.o: src/proc/context.asm
	$(AS) -f elf32 $< -o $@

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: kernel_entry.o kernel.o src/arch/io.o src/drivers/driver.o src/drivers/vga.o src/drivers/vga_gfx.o src/drivers/keyboard.o src/graphics/gfx.o src/sys/init.o src/sys/panic.o src/sys/log.o src/sys/power.o src/sys/services.o src/sys/watchdog.o src/terminal/terminal.o src/shell/shell.o src/mm/heap.o src/proc/process.o src/proc/context.o linker.ld
	$(LD) $(LDFLAGS) -o $@ kernel_entry.o kernel.o src/arch/io.o src/drivers/driver.o src/drivers/vga.o src/drivers/vga_gfx.o src/drivers/keyboard.o src/graphics/gfx.o src/sys/init.o src/sys/panic.o src/sys/log.o src/sys/power.o src/sys/services.o src/sys/watchdog.o src/terminal/terminal.o src/shell/shell.o src/mm/heap.o src/proc/process.o src/proc/context.o

$(KERNEL): kernel.elf
	$(OBJCOPY) -O binary $< $@

$(OS_IMAGE): $(BOOTLOADER) $(KERNEL)
	@size=$$(stat -f%z $(KERNEL) 2>/dev/null || stat -c%s $(KERNEL)); \
	sectors=$$(( (size + 511) / 512 )); \
	count=$$((sectors + 1)); \
	dd if=/dev/zero of=$@ bs=512 count=$$count; \
	dd if=$(BOOTLOADER) of=$@ conv=notrunc; \
	dd if=$(KERNEL) of=$@ seek=1 conv=notrunc

run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE)

clean:
	rm -f *.o *.elf src/arch/*.o src/drivers/*.o src/graphics/*.o src/sys/*.o src/terminal/*.o src/shell/*.o src/mm/*.o src/proc/*.o $(BOOTLOADER) $(KERNEL) $(OS_IMAGE)
