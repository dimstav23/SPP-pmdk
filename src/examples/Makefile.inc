# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# examples/Makefile.inc -- build the Persistent Memory Development Kit examples
#
TOP_SRC := $(dir $(lastword $(MAKEFILE_LIST)))..
TOP := $(TOP_SRC)/..
HEADERS = $(wildcard *.h) $(wildcard *.hpp)

INCDIR = $(TOP_SRC)/include
LIBDIR = $(TOP_SRC)/debug

include $(TOP)/src/common.inc

CXXFLAGS = -std=c++11 -ggdb -Wall -Werror
CXXFLAGS += $(GLIBC_CXXFLAGS)
CXXFLAGS +=  $(EXTRA_CXXFLAGS)
CFLAGS = -std=gnu99 -ggdb -Wall -Werror -Wmissing-prototypes $(EXTRA_CFLAGS)
LDFLAGS = -Wl,-rpath=$(LIBDIR) -L$(LIBDIR) $(EXTRA_LDFLAGS)
ifneq ($(SANITIZE),)
CFLAGS += -fsanitize=$(SANITIZE)
CXXFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
endif
INCS = -I$(INCDIR) -I. -I$(TOP_SRC)/examples $(OS_INCS)
LIBS += $(OS_LIBS) $(LIBUUID)

OPT_LEVEL:=-O1

#if SPP_OFF is set, enable PMDK's original functions
ifeq ($(SPP_OFF),1) #----------------------------------
CFLAGS += -DSPP_OFF
CXXFLAGS += -DSPP_OFF
else #----------------------------------
SPPROOT:= $(TOP)/..
LLVMROOT:=$(SPPROOT)/llvm-project
SPPLIB:=$(SPPROOT)/runtime
SPPLIBSRC:=$(SPPLIB)/src
SPPLIBOBJ:=$(SPPLIB)/obj

# https://llvm.org/docs/GoldPlugin.html#quickstart-for-using-lto-with-autotooled-projects
CC:=$(shell which clang)
CXX:=$(shell which clang++)
AR:=$(shell which llvm-ar)
NM:=$(shell which llvm-nm)
#LD:=$(shell which ld.gold)

WRAP_LIST:=-Wl,-wrap,free -Wl,-wrap,strcpy -Wl,-wrap,strcmp \
           -Wl,-wrap,strncpy -Wl,-wrap,strncmp -Wl,-wrap,memcmp \
           -Wl,-wrap,memchr -Wl,-wrap,strchr -Wl,-wrap,strncat \
           -Wl,-wrap,strtol -Wl,-wrap,strlen -Wl,-wrap,strchrnul \
           -Wl,-wrap,strcat -Wl,-wrap,snprintf \
           -Wl,-wrap,memcpy -Wl,-wrap,memset -Wl,-wrap,memmove \
           -Wl,-wrap,pmem_memmove_persist -Wl,-wrap,pmem_memcpy_persist \
           -Wl,-wrap,pmem_memmove_nodrain -Wl,-wrap,pmem_memcpy_nodrain \
           -Wl,-wrap,pmem_memmove -Wl,-wrap,pmem_memcpy \
           -Wl,-wrap,pmem_memset_nodrain -Wl,-wrap,pmem_memset \
           -Wl,-wrap,pmem_memset_persist -Wl,-wrap,pmemobj_memcpy \
           -Wl,-wrap,pmemobj_memcpy_persist -Wl,-wrap,pmemobj_memmove \
           -Wl,-wrap,pmemobj_memset -Wl,-wrap,pmemobj_memset_persist
FORTIFY:=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
LDFLAGS += -lpmem

CFLAGS += -flto $(OPT_LEVEL) $(FORTIFY)
CFLAGS += -Xclang -load -Xclang $(LLVMROOT)/build/lib/LLVMSPP.so
BUILD_EXEC_FLAGS := $(WRAP_LIST) $(SPPLIBOBJ)/wrappers_spp.o -Xlinker "${SPPLIBOBJ}/spp.o"

INCS += -I$(SPPLIBSRC)/spp.h
LDFLAGS += -fuse-ld=gold
endif #----------------------------------

#if TAG_BITS is defined, propagate its value
ifneq ($(TAG_BITS), )
CFLAGS += -DTAG_BITS=$(TAG_BITS)
CXXFLAGS += -DTAG_BITS=$(TAG_BITS)
endif


LINKER=$(CC)
ifeq ($(COMPILE_LANG), cpp)
LINKER=$(CXX)
endif

all-dirs:     TARGET = all
clean-dirs:   TARGET = clean
clobber-dirs: TARGET = clobber
cstyle-dirs:  TARGET = cstyle
format-dirs:  TARGET = format
sparse-dirs:  TARGET = sparse

all: $(if $(DIRS), all-dirs) $(if $(LIBRARIES), all-libraries) $(if $(PROGS), all-progs)
clean: $(if $(DIRS), clean-dirs) $(if $(PROGS), clean-progs) $(if $(LIBRARIES), clean-libraries)
clobber: $(if $(DIRS), clobber-dirs) $(if $(PROGS), clobber-progs) $(if $(LIBRARIES), clobber-libraries)
cstyle: $(if $(DIRS), cstyle-dirs)
format: $(if $(DIRS), format-dirs)
sparse: $(if $(DIRS), sparse-dirs)
	$(if $(DIRS), , $(sparse-c))

DYNAMIC_LIBRARIES = $(addprefix lib, $(addsuffix .so, $(LIBRARIES)))
STATIC_LIBRARIES = $(addprefix lib, $(addsuffix .a, $(LIBRARIES)))

all-dirs clean-dirs clobber-dirs cstyle-dirs format-dirs sparse-dirs: $(DIRS)
all-progs: $(PROGS)

ifeq ($(SPP_OFF),1) #----------------------------------
all-libraries: $(DYNAMIC_LIBRARIES) $(STATIC_LIBRARIES)
$(foreach l, $(LIBRARIES), $(eval lib$(l).so: lib$(l).o))
$(foreach l, $(LIBRARIES), $(eval $(l): lib$(l).so lib$(l).a))
else #----------------------------------
all-libraries: $(STATIC_LIBRARIES)
$(foreach l, $(LIBRARIES), $(eval $(l): lib$(l).a))
endif #----------------------------------

$(foreach l, $(LIBRARIES), $(eval lib$(l).a: lib$(l).o))
$(foreach l, $(LIBRARIES), $(eval lib$(l).o: CFLAGS+=-fPIC))
$(foreach l, $(LIBRARIES), $(eval lib$(l).o: CXXFLAGS+=-fPIC))
$(foreach l, $(LIBRARIES), $(eval .PHONY: $(l)))

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

clobber-progs: clean-progs
clobber-libraries: clean-libraries

clobber-progs clobber-libraries:
ifneq ($(PROGS),)
	$(RM) $(PROGS)
endif
ifneq ($(LIBRARIES),)
	$(RM) $(DYNAMIC_LIBRARIES) $(STATIC_LIBRARIES)
endif

clean-progs clean-libraries:
	$(RM) *.o $(TMP_HEADERS)

MAKEFILE_DEPS=Makefile $(TOP)/src/examples/Makefile.inc $(TOP)/src/common.inc

ifneq ($(HEADERS),)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif
endif

all: $(TMP_HEADERS)

%.o: %.cpp $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	$(CXX) -c -o $@ $(CXXFLAGS) $(INCS) $<

%.htmp: %.h
	$(call check-cstyle, $<, $@)

%.hpptmp: %.hpp
	$(call check-cstyle, $<, $@)

ifeq ($(SPP_OFF),1) #----------------------------------

%.o: %.c $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	$(CC) -c $(OPT_LEVEL) -o $@ $(CFLAGS) $(INCS) $<

lib%.o:
	$(LD) $(OPT_LEVEL) -o $@ -r $^

$(STATIC_LIBRARIES):
	$(AR) rv $@ $<

$(DYNAMIC_LIBRARIES):
	$(LINKER) $(OPT_LEVEL) -shared -o $@ $(LDFLAGS) -Wl,-shared,-soname=$@ $(LIBS) $<

$(PROGS): | $(TMP_HEADERS)
	LD_LIBRARY_PATH=$(LIBFABRIC_LD_LIBRARY_PATHS):$(LIBNDCTL_LD_LIBRARY_PATHS):$(LD_LIBRARY_PATH) $(LINKER) $(OPT_LEVEL) -o $@ $^ $(LDFLAGS) $(LIBS)

else #----------------------------------

%.o: %.c $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	$(CC) -c -o $@.ll $(INCS) -I../ -I../hashmap -I../tree_map -I../list_map -O1 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -S -emit-llvm $< 
	opt -O1 -load $(LLVMROOT)/build/lib/LLVMSPP.so -S $@.ll -o $@.transformed.ll
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

lib%.o:
	cp $^ $@

libmap%.o:
	$(AR) rcv $@ $^

libmap.o:
	$(AR) rcv $@ $^

$(STATIC_LIBRARIES):
	@if [[ $@ == "libmap"*".a" ]]; then \
		echo "cp $^ $@"; \
		cp $^ $@; \
	else \
		echo "$(AR) rcv $@ $<"; \
		$(AR) rcv $@ $<; \
	fi

$(PROGS): | $(TMP_HEADERS)
	LD_LIBRARY_PATH=$(LIBFABRIC_LD_LIBRARY_PATHS):$(LIBNDCTL_LD_LIBRARY_PATHS):$(LD_LIBRARY_PATH) $(LINKER) $(CFLAGS) $(BUILD_EXEC_FLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

endif #----------------------------------

.PHONY: all clean clobber cstyle\
	all-dirs clean-dirs clobber-dirs cstyle-dirs\
	all-progs clean-progs clobber-progs cstyle-progs\
	$(DIRS)
