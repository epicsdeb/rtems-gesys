#
#  Makefile,v 1.44 2004/11/09 04:02:33 till Exp
#
#

# On a PC console (i.e. VGA, not a serial terminal console)
# tecla should _not_ be used because the RTEMS VGA console
# driver is AFAIK non-ANSI and too dumb to be used by tecla.
# Don't forget to configure Cexp with --disable-tecla in this
# case...
# UPDATE: I just filed PR#502 to RTEMS-4.6.0pre4 - if you
#         have this, libtecla should work fine (2003/9/26)
# On a PC, you may have to use a different network driver
# also; YMMV. The value must be 'YES' or 'NO' (no quotes) 
USE_TECLA      = YES
# Whether to use the libbspExt library. This is always
# (automagically) disabled on pcx86 BSPs.
USE_BSPEXT     = $(shell if test -d ../libbspExt; then echo YES; else echo NO; fi)
# Whether to use the 'netboot' library (on BSPs where this applies)
USE_LIBNETBOOT = $(shell if test -d ../netboot; then echo YES; else echo NO; fi)

#
USE_BSDNETDRVS = $(shell if test -d ../bsd_eth_drivers; then echo YES ; else echo NO; fi)

# Include NFS support; system symbol table and initialization
# scripts can be loaded using NFS
USE_NFS        = YES

# Include TFTP filesystem support; system symbol table and
# initialization scripts can be loaded using TFTP
USE_TFTPFS     = YES

# Include RSH support for downloading the system symbol
# table and a system initialization script (user level
# script not supported).
# NOTE: RSH support is NOT a filesystem but just downloads
#       the essential files (symfile and script) to the IMFS,
#       i.e., NO path is available to the script.
USE_RSH        = YES

# Whether the Cexp symbol table should be built into the executable ('YES')
# If 'NO', cexp has to read the symbol table from a separate (.sym) file.
# If 'YES', the Cexp source directory is assumed to be found in
# the parent directory (for searching -I../cexp for cexpsyms.h)
USE_BUILTIN_SYMTAB = YES

# Whether to include the RTC driver (if available; BSPs w/o one
# should automatically omit it)
USE_RTC_DRIVER = YES

# These are local and experimental debugging tools - do not
# enable unless you know what you are doing.
# Both cannot used at the same time
# USE_MDBG   = YES
# USE_EFENCE = YES

# Optional libraries to add, i.e. functionality you
# want to be present but which is not directly used
# by GeSys itself.
#
# By default, GeSys links in as much as it can from
# any library it knows of (libc, librtemscpu, OPT_LIBRARIES
# etc. you can list objects you want to exclude explicitely
# in the EXCLUDE_LISTS below.
OPT_LIBRARIES = -lm -lrtems++

# Particular objects to EXCLUDE from the link.
# '$(NM) -fposix -g' generated files are acceptable
# here. You can e.g. copy $(ARCH)/librtemscpu.nm 
# (generated during a first run) to librtemscpu.excl
# and edit it to only contain stuff you DONT want.
# Then mention it here
# 
EXCLUDE_LISTS+=$(wildcard config/*.excl)

# This was permantently excluded from librtemsbsp.a by a patch
#EXCLUDE_LISTS+=config/libnetapps.excl  

#
# Particular objects to INCLUDE with the link (note that
# dependencies are also added).
#
# Note: INCLUDE_LISTS override EXCLUDE_LISTS; if you need more
#       fine grained control, you can pass include/exclude list
#       files explicitely  to 'ldep' and they are processed
#       in the order they appear on the command line.
INCLUDE_LISTS+=$(wildcard config/*.incl)

# RTEMS versions older than 4.6.0 (and probably newer than 4.5)
# have a subtle bug (PR504). Setting to YES installs a fix for this.
# This should be NO on 4.6.0 and later.
USE_GC=NO


# C source names, if any, go here -- minus the .c
# make a system for storing in flash. The compressed binary
# can be generated with the tools and code from 'netboot'.
# Note that this is currently limited to compressed images < 512k
#
#C_PIECES=flashInit rtems_netconfig config flash
#


# Normal (i.e. non-flash) system which can be net-booted
USE_TECLA_YES_C_PIECES = term
C_PIECES=init rtems_netconfig config addpath ctrlx $(USE_TECLA_$(USE_TECLA)_C_PIECES)
C_PIECES_USE_RTC_DRIVER_YES=missing
C_PIECES+=$(C_PIECES_USE_RTC_DRIVER_$(USE_RTC_DRIVER))

# SSRL 4.6.0pre2 compatibility workaround. Obsolete.
#C_PIECES+=pre2-compat

C_FILES=$(C_PIECES:%=%.c)
C_O_FILES=$(C_PIECES:%=${ARCH}/%.o)

# C++ source names, if any, go here -- minus the .cc
CC_PIECES=

CC_PIECES_GC_YES=gc
CC_PIECES+=$(CC_PIECES_GC_$(USE_GC))

CC_FILES=$(CC_PIECES:%=%.cc)
CC_O_FILES=$(CC_PIECES:%=${ARCH}/%.o)

H_FILES=

# Assembly source names, if any, go here -- minus the .S
S_PIECES=
S_FILES=$(S_PIECES:%=%.S)
S_O_FILES=$(S_FILES:%.S=${ARCH}/%.o)

SRCS=$(C_FILES) $(CC_FILES) $(H_FILES) $(S_FILES)
OBJS=$(C_O_FILES) $(CC_O_FILES) $(S_O_FILES)

PGMS=${ARCH}/rtems.exe ${ARCH}/st.sys

# List of RTEMS managers to be included in the application goes here.
# Use:
#     MANAGERS=all
# to include all RTEMS managers in the application.
MANAGERS=all


include $(RTEMS_MAKEFILE_PATH)/Makefile.inc
include $(RTEMS_CUSTOM)
include $(RTEMS_ROOT)/make/leaf.cfg

ifndef XSYMS
XSYMS = $(RTEMS_CPU)-rtems-xsyms
endif


#
# (OPTIONAL) Add local stuff here using +=
#
# make-exe uses LINK.c but we have C++ stuff
LINK.c = $(LINK.cc)

DEFINES_BSPEXT_YES=-DHAVE_LIBBSPEXT
#DEFINES  += -DCDROM_IMAGE 
DEFINES  += -DUSE_POSIX
DEFINES  += $(DEFINES_BSPEXT_$(USE_BSPEXT))

DEFINES_USE_RTC_DRIVER_YES=-DUSE_RTC_DRIVER
DEFINES += $(DEFINES_USE_RTC_DRIVER_$(USE_RTC_DRIVER))

# Trim BSP specific things
#
# bsps w/o netboot use local 'pairxtract'
ifeq "$(filter $(RTEMS_BSP_FAMILY),svgm beatnik uC5282 mvme3100)xx" "xx"
USE_LIBNETBOOT=NO
endif

ifeq "$(USE_LIBNETBOOT)" "YES"
DEFINES  += -DHAVE_LIBNETBOOT
LD_LIBS  += -lnetboot
else
C_PIECES  += pairxtract
endif

# pieces for the 'mdbg' memory leak debugger
C_PIECES_MDBG_YES = mdbg
LDFLAGS_MDBG_YES  = -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,realloc -Wl,--wrap,calloc
LDFLAGS_MDBG_YES += -Wl,--wrap,_malloc_r -Wl,--wrap,_free_r -Wl,--wrap,_realloc_r -Wl,--wrap,_calloc_r

C_PIECES += $(C_PIECES_MDBG_$(USE_MDBG))
LDFLAGS  += $(LDFLAGS_MDBG_$(USE_MDBG))

# pieces for the 'efence' heap corruption debugger
# (accesses outside of malloced areas are trapped;
# need PPC 604 paging hardware for this!!)
#
ifneq "$(filter $(RTEMS_BSP_FAMILY),svgm beatnik mvme5500)xx" "xx"
USE_EFENCE=NO
endif

LD_LIBS_EFENCE_YES  += -lefence
LDFLAGS_EFENCE_YES   = -Wl,--wrap,malloc -Wl,--wrap,realloc -Wl,--wrap,calloc -Wl,--wrap,free
LDFLAGS_EFENCE_YES  += -Wl,--wrap,_malloc_r -Wl,--wrap,_realloc_r -Wl,--wrap,_calloc_r -Wl,--wrap,_free_r

LD_LIBS += $(LD_LIBS_EFENCE_$(USE_EFENCE))
LDFLAGS += $(LDFLAGS_EFENCE_$(USE_EFENCE))

# BSP specifica...

ifneq "$(filter $(RTEMS_BSP_FAMILY),svgm beatnik)xx" "xx"
DEFINES  += -DHAVE_BSP_EXCEPTION_EXTENSION
endif 

ifneq "$(filter $(RTEMS_BSP_FAMILY),svgm beatnik mvme3100)xx" "xx"
DEFINES  += "-DEARLY_CMDLINE_GET(arg)=do { *(arg) = BSP_commandline_string; } while (0)"
endif

ifeq "$(RTEMS_BSP_FAMILY)" "psim"
USE_BSPEXT = NO
USE_TFTPFS = NO
USE_NFS    = NO
USE_RSH    = NO
C_PIECES  += bug_disk
DEFINES   += -DPSIM
INSTFILES += psim_tree.gesys
endif

ifeq "$(RTEMS_BSP_FAMILY)" "motorola_powerpc"
DEFINES  += -DRTEMS_BSP_NETWORK_DRIVER_NAME=\"dc1\"
DEFINES  += -DRTEMS_BSP_NETWORK_DRIVER_ATTACH=rtems_dec21140_driver_attach
endif

ifeq "$(RTEMS_BSP)" "beatnik"
DEFINES+=-DMEMORY_HUGE
endif

ifeq  "$(RTEMS_BSP_FAMILY)" "pc386"
ifeq  "$(USE_BSDNETDRVS)"   "YES"
LD_LIBS  += -lif_pcn
LD_LIBS  += -lif_em
LD_LIBS  += -lif_le
LD_LIBS  += -lbsdport
DEFINES  += -DNIC_NAME='""' -DNIC_ATTACH=libbsdport_netdriver_attach
else
DEFINES  += -DMULTI_NETDRIVER
endif
DEFINES  += -DHAVE_PCIBIOS
DEFINES  += -DMEMORY_HUGE
DEFINES  += "-DEARLY_CMDLINE_GET(arg)=do { *(arg) = BSP_commandline_string; } while (0)"
ifndef ELFEXT
ELFEXT    = obj
endif
####### OBSOLETE AS OF PR#624 #######
#define bsp-size-check
#	@if [ `$(SIZE_FOR_TARGET) $(@:%.exe=%.$(ELFEXT)) | awk '/2/{print $$4}'` -ge 2097148 ]; then \
#		echo '*******************************************************************************'; \
#		echo "Your binary is so large (>=~2M) that the BSP's memory detection would write into it!";\
#		echo "Change the probing limits in c/src/lib/libbsp/i386/pc386/startup/bspstart.c, rebuild/install";\
#		echo "RTEMS and modify this check accordingly..." ;\
#		echo '*******************************************************************************'; \
#		$(RM) $@ $(@:%.exe=%.$(ELFEXT)) ; \
#		exit 1;\
#	fi
#endef
endif

ifneq "$(filter $(RTEMS_BSP_FAMILY),mvme167 uC5282)xx" "xx"
USE_BSPEXT = NO
endif

ifneq "$(filter $(RTEMS_BSP_FAMILY),mvme167)xx" "xx"
DEFINES+='-DMEMORY_SCARCE=(1024*1024)'
endif

ifneq "$(filter $(RTEMS_BSP_FAMILY),uC5282)xx" "xx"
DEFINES+='-DMEMORY_SCARCE=(2*1024*1024)'
C_PIECES+=bev reboot5282
ifeq "$(USE_LIBNETBOOT)" "YES" 
DEFINES+= "-DEARLY_CMDLINE_GET(arg)=do { *(arg) = 0; /* use internal buffer */ } while (0)"
else
DEFINES+=-DBSP_NETWORK_SETUP=bev_network_setup
endif
endif

# RTEMS 4.8.99 has 'unified' the ELF extension to be '.exe'
# which is hardcoded ATM; they also define a 'DOWNEXT' variable
# (extension of downloadable file). We test for presence of
# 'DOWNEXT' to find out if we're using the new system...

ifndef DOWNEXT
ifndef ELFEXT
ELFEXT=exe
endif
else
ELFEXT=exe
endif

DOWNEXT=.bin

bspfail:
	$(error GeSys has not been ported/tested on this BSP ($(RTEMS_BSP)) yet)

bspcheck: $(if $(filter $(RTEMS_BSP_FAMILY),pc386 motorola_powerpc svgm mvme5500 beatnik mvme3100 mvme167 uC5282 psim),,bspfail)


CPPFLAGS += -I. -Invram -DHAVE_CEXP
ifeq "$(USE_BUILTIN_SYMTAB)xx" "YESxx"
CPPFLAGS += -I../cexp
endif
CFLAGS   += -O2
# Enable the stack checker. Unfortunately, this must be
# a command line option because some pieces are built into
# the system configuration table...
#CFLAGS   +=-DSTACK_CHECKER_ON

USE_TECLA_YES_DEFINES  = -DWINS_LINE_DISC -DUSE_TECLA
USE_NFS_YES_DEFINES    = -DNFS_SUPPORT
USE_TFTPFS_YES_DEFINES = -DTFTP_SUPPORT
USE_RSH_YES_DEFINES    = -DRSH_SUPPORT

DEFINES+=$(USE_TECLA_$(USE_TECLA)_DEFINES)
DEFINES+=$(USE_NFS_$(USE_NFS)_DEFINES)
DEFINES+=$(USE_TFTPFS_$(USE_TFTPFS)_DEFINES)
DEFINES+=$(USE_RSH_$(USE_RSH)_DEFINES)

#
# CFLAGS_DEBUG_V are used when the `make debug' target is built.
# To link your application with the non-optimized RTEMS routines,
# uncomment the following line:
# CFLAGS_DEBUG_V += -qrtems_debug
#

USE_TECLA_YES_LIB  = -ltecla_r
USE_BSPEXT_YES_LIB = -lbspExt
USE_NFS_YES_LIB    = -lrtemsNfs

LD_LIBS   += -lcexp -lbfd -lspencer_regexp -lopcodes -liberty
LD_LIBS   += $(USE_TECLA_$(USE_TECLA)_LIB)
LD_LIBS   += $(USE_BSPEXT_$(USE_BSPEXT)_LIB)
LD_LIBS   += $(USE_NFS_$(USE_NFS)_LIB)
LD_LIBS   += $(OPT_LIBRARIES)

# Produce a linker map to help finding 'undefined symbol' references (README.config)
LDFLAGS_GC_YES = -Wl,--wrap,free
LDFLAGS   += -Wl,-Map,$(ARCH)/linkmap $(LDFLAGS_GC_$(USE_GC)) -Wl,--trace-symbol,__end
##LDFLAGS += -Tlinkcmds

# this special object contains 'undefined' references for
# symbols we want to forcibly include. It is automatically
# generated. 
OBJS      += ${ARCH}/allsyms.o

#
# Add your list of files to delete here.  The config files
#  already know how to delete some stuff, so you may want
#  to just run 'make clean' first to see what gets missed.
#  'make clobber' already includes 'make clean'
#

CLEAN_ADDITIONS   += builddate.c pathcheck.c pairxtract.c ctrlx.c
CLOBBER_ADDITIONS +=

THELIBS:=$(shell $(LINK.cc) $(AM_CFLAGS) $(AM_LDFLAGS) $(LINK_LIBS) -specs=myspec)
vpath %.a $(patsubst -L%,%,$(filter -L%,$(THELIBS)))

all: bspcheck gc-check libnms ${ARCH} $(SRCS) $(PGMS)

# We want to have the build date compiled in...
$(ARCH)/init.o: builddate.c pathcheck.c ctrlx.c

builddate.c: $(filter-out $(ARCH)/init.o $(ARCH)/allsyms.o,$(OBJS)) Makefile
	echo 'static char *system_build_date="'`date +%Y%m%d%Z%T`'";' > $@
	echo '#define DEFAULT_CPU_ARCH_FOR_CEXP "'`$(XSYMS) -a $<`'"' >>$@

pathcheck.c: nvram/pathcheck.c
	$(LN) -s $^ $@

ctrlx.c: nvram/ctrlx.c
	$(LN) -s $^ $@

# a hack for now
pairxtract.c:	nvram/pairxtract.c
	$(LN) -s $^ $@


#LINK_OBJS+= -Wl,-b,binary o-optimize/rtems.sym.tar -Wl,-b,elf32-i386

# Build the executable and a symbol table file
$(filter %.exe,$(PGMS)): ${LINK_FILES}
	$(make-exe)
	$(bsp-size-check)
ifeq "$(USE_BUILTIN_SYMTAB)xx" "NOxx"
ifdef ELFEXT
ifdef XSYMS
ifeq ($(USE_GC),YES)
	$(OBJCOPY) --redefine-sym free=__real_free --redefine-sym __wrap_free=free $(@:%$(DOWNEXT)=%.$(ELFEXT))
endif
	$(XSYMS) $(@:%$(DOWNEXT)=%.$(ELFEXT)) $(@:%$(DOWNEXT)=%.sym)
endif
endif
endif

# Installation
ifndef RTEMS_SITE_INSTALLDIR
RTEMS_SITE_INSTALLDIR = $(PROJECT_RELEASE)
endif

$(RTEMS_SITE_INSTALLDIR)/bin:
	test -d $@ || mkdir -p $@

INSTFILES += ${PGMS} ${PGMS:%.exe=%.$(ELFEXT)} ${PGMS:%.exe=%.bin} ${PGMS:%.exe=%.sym}

# How to build a  tarball of this package
REVISION=$(filter-out $$%,$$Name: GeSys_2_4 $$)
tar:
	@$(make-tar)

# Install the program(s), appending _g or _p as appropriate.
# for include files, just use $(INSTALL_CHANGE)
install: all $(RTEMS_SITE_INSTALLDIR)/bin
	for feil in $(INSTFILES); do if [ -r $$feil ] ; then  \
		$(INSTALL_VARIANT) -m 555 $$feil ${RTEMS_SITE_INSTALLDIR}/bin ; fi ; done


# Below here, magic follows for generating the
# 'allsyms.o' object
#

# Our tool; should actually already be defined to
# point to a site specific installated version.
ifndef LDEP
LDEP = ldep/ldep
# emergency build
ldep/ldep: ldep/ldep.c
	cc -O -o $@ $<
CLEAN_ADDITIONS+=ldep/ldep
endif


# Produce an object with undefined references.
# These are listed in the linker script generated
# by LDEP.
SYMLIST_LDS = $(ARCH)/ldep.lds
# We just need an empty object to keep the linker happy

ifeq "$(USE_BUILTIN_SYMTAB)xx" "NOxx"
$(ARCH)/allsyms.o:	$(SYMLIST_LDS) $(ARCH)/empty.o
	$(LD) -T$< -r -o $@ $(ARCH)/empty.o
endif

# dummy up an empty object file
$(ARCH)/empty.o:
	touch $(@:%.o=%.c)
	$(CC) -c -O -o $@ $(@:%.o=%.c)

# try to find out what startfiles will be linked in
# and what symbols are defined by the linker script
$(ARCH)/gcc-startfiles.$(ELFEXT):
	$(LINK.cc) -nodefaultlibs -o $@ -Wl,--unresolved-symbols=ignore-all -T`$(CC) -print-file-name=linkcmds`

# and generate a name file for them (the endfiles will
# actually be there also)
$(ARCH)/startfiles.nm: $(ARCH)/gcc-startfiles.$(ELFEXT)
	$(NM) -g -fposix $^ > $@

# generate a name file for the application's objects
$(ARCH)/app.nm: $(filter-out $(ARCH)/allsyms.o,$(OBJS))
	$(NM) -g -fposix $^ > $@

# NOTE: must not make 'myspec'! Otherwise, the first
#       'make' won't find it when interpreting the makefile.
#
#myspec:
#	$(RM) $@
#	echo '*linker:' >$@
#	echo "`pwd`/mylink" >>$@
#


LIBNMS=$(patsubst %.a,$(ARCH)/%.nm,$(sort $(patsubst -l%,lib%.a,$(filter -l%,$(THELIBS)))))
OPTIONAL_ALL=$(addprefix -o,$(LIBNMS)) 
#OPTIONAL_ALL=-o$(ARCH)/app.nm

$(SYMLIST_LDS): $(ARCH)/app.nm $(LIBNMS) $(ARCH)/startfiles.nm $(EXCLUDE_LISTS) $(LDEP)
	echo $^
	$(LDEP) -F -l -u $(OPTIONAL_ALL) $(addprefix -x,$(EXCLUDE_LISTS)) $(addprefix -o,$(INCLUDE_LISTS)) -e $@ $(filter %.nm,$^)  > $(ARCH)/ldep.log

$(ARCH)/allsyms.c: $(ARCH)/app.nm $(LIBNMS) $(ARCH)/startfiles.nm $(EXCLUDE_LISTS) $(LDEP)
	echo $^
	$(LDEP) -F -l -u $(OPTIONAL_ALL) $(addprefix -x,$(EXCLUDE_LISTS)) $(addprefix -o,$(INCLUDE_LISTS)) -C $@ $(filter %.nm,$^)  > $(ARCH)/ldep.log


libnms: $(ARCH) $(LIBNMS)
	
$(ARCH)/%.nm: %.a
	$(NM) -g -fposix $^ > $@



foo:
	echo $(filter %.nm,$(LIBNMS))

thelibs:
	@echo THELIBS:
	@echo $(THELIBS)
	@echo
	@echo Library vpath:
	@echo $(patsubst -L%,%,$(filter -L%,$(THELIBS)))

#initialization script
$(ARCH)/st.sys: st.sys $(wildcard st.sys-ssrl) $(wildcard st.sys-$(RTEMS_BSP)) $(wildcard st.sys-$(RTEMS_BSP)-ssrl)
	cat $^ | sed -e 's/@RTEMS_CPU@/$(RTEMS_CPU)/g' -e 's/@RTEMS_BSP@/$(RTEMS_BSP)/g' > $@



# Create the name files for our libraries. This is achieved by
# invoking the compiler with a special 'spec' file which instructs
# it to use a dummy "linker" ('mylink' shell script). 'mylink'
# then extracts '-L' and '-l' linker command line options to
# localize all libraries we use.
# Finally, we recursively invoke this Makefile again, now
# knowing the libraries.
#libnms: $(ARCH)
#	sh -c "$(MAKE) `$(LINK.cc) $(AM_CFLAGS) $(AM_LDFLAGS) $(LINK_LIBS) -specs=myspec` libnms-recurse"

# 4.6. pre versions require the GC workaround. 4.5 probably not, but we
# don't bother...
gc-check: librtemscpu.a
ifeq ($(USE_GC),YES)
	@if nm $^ | grep -s RTEMS_Malloc_GC_list > /dev/null ; then \
		echo 'Your RTEMS release has bug #504 apparently fixed; set USE_GC to NO' ;\
		exit 1;\
	fi
else
	@if ! nm $^ | grep -s RTEMS_Malloc_GC_list > /dev/null ; then \
		echo 'Your RTEMS release might have bug #504; set USE_GC to YES' ;\
		exit 1;\
	fi
endif

prlinkcmds:
	$(CC) -print-file-name=linkcmds

tst$(DOWNEXT):OBJS=
tst$(DOWNEXT):LD_LIBS=

tst$(DOWNEXT):
	$(LINK.cc) -nodefaultlibs -o $@ -Wl,--unresolved-symbols=ignore-all -print-file-name=linkcmds

blah: 
	echo $(LINK_FILES)
