#  Copyright (C) 2024 Free Software Foundation

# This program is free software ; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation ; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY ; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the program ; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#
# MIG stubs generation for user-space tests
#

MACH_TESTINSTALL = $(builddir)/tests/include-mach
MACH_TESTINCLUDE = $(MACH_TESTINSTALL)/$(includedir)

MIGCOMUSER = $(USER_MIG) -n -cc cat - /dev/null
MIG_OUTDIR = $(builddir)/tests/mig-out
MIG_CPPFLAGS = -x c -nostdinc -I$(MACH_TESTINCLUDE)

# FIXME: how can we reliably detect a change on any header and reinstall them?
$(MACH_TESTINSTALL):
	mkdir -p $@
	$(MAKE) install-data DESTDIR=$@

prepare-test: $(MACH_TESTINSTALL)

$(MIG_OUTDIR):
	mkdir -p $@

define generate_mig_client
$(MACH_TESTINCLUDE)/$(1)/$(2).defs: $(MACH_TESTINSTALL)
$(MIG_OUTDIR)/$(2).user.c: prepare-test $(MIG_OUTDIR) $(MACH_TESTINCLUDE)/$(1)/$(2).defs
	$(USER_CPP) $(USER_CPPFLAGS) $(MIG_CPPFLAGS) 	\
		-o $(MIG_OUTDIR)/$(2).user.defs 	\
		$(MACH_TESTINCLUDE)/$(1)/$(2).defs
	$(MIGCOMUSER) $(MIGCOMFLAGS) $(MIGCOMUFLAGS)	\
		-user $(MIG_OUTDIR)/$(2).user.c		\
		-header $(MIG_OUTDIR)/$(2).user.h	\
		-list $(MIG_OUTDIR)/$(2).user.msgids	\
		< $(MIG_OUTDIR)/$(2).user.defs
endef

define generate_mig_server
$(MACH_TESTINCLUDE)/$(1)/$(2).defs: $(MACH_TESTINSTALL)
$(MIG_OUTDIR)/$(2).server.c: prepare-test $(MIG_OUTDIR) $(srcdir)/include/$(1)/$(2).defs
	$(USER_CPP) $(USER_CPPFLAGS) $(MIG_CPPFLAGS) 	\
		-o $(MIG_OUTDIR)/$(2).server.defs 	\
		$(srcdir)/include/$(1)/$(2).defs
	$(MIGCOMUSER) $(MIGCOMFLAGS) $(MIGCOMUFLAGS)	\
		-server $(MIG_OUTDIR)/$(2).server.c	\
		-header $(MIG_OUTDIR)/$(2).server.h	\
		-list $(MIG_OUTDIR)/$(2).server.msgids	\
		< $(MIG_OUTDIR)/$(2).server.defs
endef

# These are all the IPC implemented in the kernel, both as a server or as a client.
# Files are sorted as in
#   find builddir/tests/include-mach/ -name *.defs | grep -v types | sort
# eval->info for debug of generated rules
$(eval $(call generate_mig_client,device,device))
$(eval $(call generate_mig_client,device,device_reply))
$(eval $(call generate_mig_client,device,device_request))
$(eval $(call generate_mig_client,mach_debug,mach_debug))
# default_pager.defs?
$(eval $(call generate_mig_server,mach,exc))
# experimental.defs?
$(eval $(call generate_mig_client,mach,gnumach))
$(eval $(call generate_mig_client,mach,mach4))
$(eval $(call generate_mig_client,mach,mach))
$(eval $(call generate_mig_client,mach,mach_host))
$(eval $(call generate_mig_client,mach,mach_port))
# memory_object{_default}.defs?
# notify.defs?
$(eval $(call generate_mig_server,mach,task_notify))
if HOST_ix86
$(eval $(call generate_mig_client,mach/i386,mach_i386))
endif
if HOST_x86_64
$(eval $(call generate_mig_client,mach/x86_64,mach_i386))
endif

# NOTE: keep in sync with the rules above
MIG_GEN_CC = \
	$(MIG_OUTDIR)/device.user.c \
	$(MIG_OUTDIR)/device_reply.user.c \
	$(MIG_OUTDIR)/device_request.user.c \
	$(MIG_OUTDIR)/mach_debug.user.c \
	$(MIG_OUTDIR)/exc.server.c \
	$(MIG_OUTDIR)/gnumach.user.c \
	$(MIG_OUTDIR)/mach4.user.c \
	$(MIG_OUTDIR)/mach.user.c \
	$(MIG_OUTDIR)/mach_host.user.c \
	$(MIG_OUTDIR)/mach_port.user.c \
	$(MIG_OUTDIR)/task_notify.server.c \
	$(MIG_OUTDIR)/mach_i386.user.c

#
# compilation of user space tests and utilities
#

TEST_START_MARKER = booting-start-of-test
TEST_SUCCESS_MARKER = gnumach-test-success-and-reboot
TEST_FAILURE_MARKER = gnumach-test-failure

TESTCFLAGS = -static -nostartfiles -nolibc \
	-ffreestanding \
	-ftrivial-auto-var-init=pattern \
	-I$(srcdir)/tests/include \
	-I$(MACH_TESTINCLUDE) \
	-I$(MIG_OUTDIR) \
	-ggdb3 \
	-DMIG_EOPNOTSUPP


TESTSRC_TESTLIB= \
	$(srcdir)/tests/syscalls.S \
	$(srcdir)/tests/start.S \
	$(srcdir)/tests/testlib.c \
	$(srcdir)/tests/testlib_thread_start.c

SRC_TESTLIB= \
	$(srcdir)/i386/i386/strings.c \
	$(srcdir)/kern/printf.c \
	$(srcdir)/kern/strings.c \
	$(srcdir)/util/atoi.c \
	$(TESTSRC_TESTLIB) \
	$(builddir)/tests/errlist.c \
	$(MIG_GEN_CC)

tests/errlist.c: $(addprefix $(srcdir)/include/mach/,message.h kern_return.h mig_errors.h)
	mkdir -p tests
	echo "/* autogenerated file */" >$@
	echo "#include <mach/message.h>" >>$@
	echo "#include <mach/kern_return.h>" >>$@
	echo "#include <mach/mig_errors.h>" >>$@
	echo "#include <testlib.h>" >>$@
	echo "#include <stddef.h>" >>$@
	echo "const char* TEST_SUCCESS_MARKER = \"$(TEST_SUCCESS_MARKER)\";" >>$@
	echo "const char* TEST_FAILURE_MARKER = \"$(TEST_FAILURE_MARKER)\";" >>$@
	echo "const char* e2s_gnumach(int err) { switch (err) {" >>$@
	grep "define[[:space:]]MIG" $(srcdir)/include/mach/mig_errors.h | \
		awk '{printf "  case %s: return \"%s\";\n", $$2, $$2}' >>$@
	grep "define[[:space:]]KERN" $(srcdir)/include/mach/kern_return.h | \
		awk '{printf "  case %s: return \"%s\";\n", $$2, $$2}' >>$@
	awk 'f;/MACH_MSG_SUCCESS/{f=1}' $(srcdir)/include/mach/message.h | \
		grep "define[[:space:]]MACH" | \
		awk '{printf "  case %s: return \"%s\";\n", $$2, $$2}' >>$@
	echo "  default: return NULL;"  >>$@
	echo "}}"  >>$@

tests/module-%: $(srcdir)/tests/test-%.c $(SRC_TESTLIB) $(MACH_TESTINSTALL)
	$(USER_CC) $(USER_CFLAGS) $(TESTCFLAGS) $< $(SRC_TESTLIB) -o $@

#
# packaging of qemu bootable image and test runner
#

GNUMACH_ARGS = console=com0
QEMU_OPTS = -m 2047 -nographic -no-reboot -boot d
QEMU_GDB_PORT ?= 1234

if HOST_ix86
QEMU_BIN = qemu-system-i386
QEMU_OPTS += -cpu pentium3-v1
endif
if HOST_x86_64
QEMU_BIN = qemu-system-x86_64
QEMU_OPTS += -cpu core2duo-v1
endif

tests/test-%.iso: tests/module-% gnumach $(srcdir)/tests/grub.cfg.single.template
	rm -rf $(builddir)/tests/isofiles-$*
	mkdir -p $(builddir)/tests/isofiles-$*/boot/grub/
	< $(srcdir)/tests/grub.cfg.single.template		\
		sed -e "s|BOOTMODULE|$(notdir $<)|g"		\
		    -e "s/GNUMACHARGS/$(GNUMACH_ARGS)/g"	\
		    -e "s/TEST_START_MARKER/$(TEST_START_MARKER)/g"	\
		>$(builddir)/tests/isofiles-$*/boot/grub/grub.cfg
	cp gnumach $< $(builddir)/tests/isofiles-$*/boot/
	grub-mkrescue -o $@ $(builddir)/tests/isofiles-$*
	rm -rf $(builddir)/tests/isofiles-$*

tests/test-%: tests/test-%.iso $(srcdir)/tests/run-qemu.sh.template
	< $(srcdir)/tests/run-qemu.sh.template			\
		sed -e "s|TESTNAME|$(subst tests/test-,,$@)|g"	\
		    -e "s/QEMU_OPTS/$(QEMU_OPTS)/g"		\
		    -e "s/QEMU_BIN/$(QEMU_BIN)/g"			\
		    -e "s/TEST_START_MARKER/$(TEST_START_MARKER)/g"	\
		    -e "s/TEST_SUCCESS_MARKER/$(TEST_SUCCESS_MARKER)/g"	\
		    -e "s/TEST_FAILURE_MARKER/$(TEST_FAILURE_MARKER)/g"	\
		>$@
	chmod +x $@

clean-test-%:
	rm -f tests/test-$*.iso tests/module-$* tests/test-$**


USER_TESTS := \
	tests/test-hello \
	tests/test-mach_host \
	tests/test-gsync \
	tests/test-mach_port \
	tests/test-vm \
	tests/test-syscalls \
	tests/test-machmsg \
	tests/test-task \
	tests/test-threads \
	tests/test-thread-state \
	tests/test-thread-state-fp

USER_TESTS_CLEAN = $(subst tests/,clean-,$(USER_TESTS))

#
# helpers for interactive test run and debug
#

run-%: tests/test-%
	$^

# don't reuse the launcher script as the timeout would kill the debug session
debug-%: tests/test-%.iso
	$(QEMU_BIN) $(QEMU_OPTS) -cdrom $< -gdb tcp::$(QEMU_GDB_PORT) -S \
		| sed -n "/$(TEST_START_MARKER)/"',$$p'
