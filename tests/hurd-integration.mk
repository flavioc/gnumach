
#### ramdisk from hurd install cd
RAMDISK_BASE_INST=/home/luca/dev/hurd/gnumach64-multiboot-files/netinst_orig
#### my ram disk
RAMDISK_BASE_MY=/home/luca/dev/hurd/gnumach64-multiboot-files/test1
RAMDISK_BASE=$(RAMDISK_BASE_INST)
HURD_DISK_RAMFS = $(RAMDISK_BASE)/ext2fs.static
HURD_EXEC_RAMFS = $(RAMDISK_BASE)/exec.static
HURD_INITRD_RAMFS = $(RAMDISK_BASE)/initrd.gz

pull-initrd:
	scp debhurd:initrd.gz $(RAMDISK_BASE_MY)/
	scp debhurd:/hurd/exec.static $(RAMDISK_BASE_MY)/
	scp debhurd:/hurd/ext2fs.static $(RAMDISK_BASE_MY)/

HURD_DISK = $(HURD_DISK_RAMFS)
HURD_EXEC = $(HURD_EXEC_RAMFS)
HURD_RUMPDISK = /home/luca/dev/hurd/gnumach64-multiboot-files/test1/rumpdisk.static
HURD_PCI_ARBITER = /home/luca/dev/hurd/gnumach64-multiboot-files/test1/pci-arbiter.static
HURD_ROOT = /home/luca/dev/hurd/preinstalled/debian-hurd-ramdisk.img
HURD_QEMU_OPTS = -m 1024 -nographic -no-reboot
#HURD_QEMU_OPTS += -M q35,accel=kvm
HURD_QEMU_OPTS += -boot d
HURD_QEMU_OPTS_RUMP = $(HURD_QEMU_OPTS) -M q35,accel=kvm
HURD_QEMU_OPTS_RUMP += -drive file=$(HURD_ROOT),index=0,media=disk,if=none,id=udisk,format=raw
#HURD_QEMU_OPTS_RUMP += -drive file=testdisk.img,index=0,media=disk,if=none,id=udisk,format=raw
HURD_QEMU_OPTS_RUMP += -device ich9-ahci,id=myahci,addr=03
HURD_QEMU_OPTS_RUMP += -device ide-hd,drive=udisk,bus=myahci.0

test-fullhurd.iso: gnumach $(srcdir)/tests/grub.cfg.hurd
	rm -rf $(top_builddir)/tests/isofiles
	mkdir -p $(top_builddir)/tests/isofiles/boot/grub/
	cp $(srcdir)/tests/grub.cfg.hurd $(top_builddir)/tests/isofiles/boot/grub/grub.cfg
	cp gnumach $(HURD_DISK) $(HURD_EXEC) $(top_builddir)/tests/isofiles/boot/
	grub-mkrescue -o $@ $(top_builddir)/tests/isofiles

testdisk.img:
	qemu-img create -f raw $@ 1G
	parted -s $@ mktable msdos
	parted -s $@ unit s mkpart primary ext2 1 2047

# HURD_QEMU_OPTS += -hda /home/luca/.local/share/libvirt/images/hurd.qcow2
run-fullhurd: test-fullhurd.iso testdisk.img
	$(QEMU) $(HURD_QEMU_OPTS) -cdrom test-fullhurd.iso | tail -n +18  # skip terminal reconfiguration

debug-fullhurd: test-fullhurd.iso
	$(QEMU) $(HURD_QEMU_OPTS) -boot d -cdrom $< -gdb tcp::$(QEMU_GDB_PORT) -S | tail -n +18

startup: $(srcdir)/tests/startup.c $(SRC_TESTLIB) $(MACH_TESTINSTALL)
	gcc $(TESTCFLAGS) $< $(SRC_TESTLIB) -o $@

# minimal initrd, for warly boot tests
initrd: startup $(HURD_DISK_RAMFS) $(HURD_EXEC_RAMFS)
	rm -rf initrd $(top_builddir)/tests/diskfiles
	truncate -s 10M initrd
	mkdir -p $(top_builddir)/tests/diskfiles/hurd/
	mkdir -p $(top_builddir)/tests/diskfiles/servers/
	touch $(top_builddir)/tests/diskfiles/servers/exec
	cp $(HURD_DISK_RAMFS) $(HURD_EXEC_RAMFS) startup $(builddir)/tests/diskfiles/hurd/
	mkfs.ext2 -b 4096 -d $(builddir)/tests/diskfiles initrd

initrd.gz: initrd
	gzip -f initrd

test-ramhurd.iso: gnumach $(srcdir)/tests/grub.cfg.ramdisk $(HURD_INITRD_RAMFS) $(HURD_DISK_RAMFS) $(HURD_EXEC_RAMFS)
	rm -rf $(top_builddir)/tests/isofiles
	mkdir -p $(top_builddir)/tests/isofiles/boot/grub
	mkdir -p $(top_builddir)/tests/isofiles/hurd
	cp $(srcdir)/tests/grub.cfg.ramdisk $(top_builddir)/tests/isofiles/boot/grub/grub.cfg
	cp gnumach $(HURD_INITRD_RAMFS) $(top_builddir)/tests/isofiles/boot/
	cp $(HURD_DISK_RAMFS) $(HURD_EXEC_RAMFS) $(top_builddir)/tests/isofiles/hurd/
	grub-mkrescue -o $@ $(top_builddir)/tests/isofiles

run-ramhurd: test-ramhurd.iso
	$(QEMU) $(HURD_QEMU_OPTS) -cdrom test-ramhurd.iso | tail -n +12  # skip terminal reconfiguration

debug-ramhurd: test-ramhurd.iso
	$(QEMU) $(HURD_QEMU_OPTS) -boot d -cdrom $< -gdb tcp::$(QEMU_GDB_PORT) -S | tail -n +18

test-fullrump.iso: gnumach $(srcdir)/tests/grub.cfg.rump
	rm -rf $(top_builddir)/tests/isofiles
	mkdir -p $(top_builddir)/tests/isofiles/boot/grub/
	cp $(srcdir)/tests/grub.cfg.rump $(top_builddir)/tests/isofiles/boot/grub/grub.cfg
	cp gnumach $(HURD_DISK) $(HURD_EXEC) $(HURD_RUMPDISK) $(HURD_PCI_ARBITER) $(top_builddir)/tests/isofiles/boot/
	grub-mkrescue -o $@ $(top_builddir)/tests/isofiles

run-fullrump: test-fullrump.iso testdisk.img
	$(QEMU) $(HURD_QEMU_OPTS_RUMP) -cdrom test-fullrump.iso | tail -n +18  # skip terminal reconfiguration

debug-fullrump: test-fullrump.iso
	$(QEMU) $(HURD_QEMU_OPTS_RUMP) -cdrom test-fullrump.iso -gdb tcp::$(QEMU_GDB_PORT) -S | tail -n +18
