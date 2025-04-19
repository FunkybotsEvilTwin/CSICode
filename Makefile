SRC_PATH = ./reaper_csurf_integrator
WDL_PATH = ./lib/WDL/WDL
vpath %.c $(WDL_PATH)
vpath %.cpp $(WDL_PATH) $(SRC_PATH) $(WDL_PATH)/swell
vpath %.mm $(WDL_PATH)/swell

OBJS = control_surface_integrator_ui.o control_surface_integrator.o main.o

CFLAGS += -pipe -fvisibility=hidden -fno-math-errno -fPIC -DPIC -Wall -Wtype-limits \
          -Wno-unused-function -Wno-multichar -Wno-unused-result -Wno-sign-compare \
          -Wno-reorder -Wno-array-bounds -Wshadow

CFLAGS += -D_FILE_OFFSET_BITS=64 -DSWELL_PROVIDED_BY_APP

ARCH := $(shell uname -m)
UNAME_S = $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
  APPNAME=reaper_csurf_integrator.dylib
  LINKEXTRA= -framework Cocoa -lobjc
  SWELL_OBJS=swell-modstub.o
  %.o : %.mm
	$(CXX) -ObjC++ $(CXXFLAGS) -c -o $@ $<
else
  APPNAME=reaper_csurf_integrator.so
  LINKEXTRA=-lpthread -ldl
  SWELL_OBJS=swell-modstub-generic.o
endif

default: $(APPNAME)

ifdef DISALLOW_WARNINGS
  CFLAGS += -Werror
endif

ifneq ($(filter arm%,$(ARCH)),)
  CFLAGS += -fsigned-char -marm
endif

ifeq ($(ARCH), aarch64)
  CXX = g++               # clang/llvm 14 has a bug on Debian aarch64, so forcing g++
  CFLAGS += -fsigned-char
endif

ifdef DEBUG
  CFLAGS += -O0 -g -D_DEBUG -DWDL_CHECK_FOR_NON_UTF8_FOPEN
else
  CFLAGS += -O2 -DNDEBUG
endif

CXXFLAGS = $(CFLAGS) -std=c++17

RESINTER = $(SRC_PATH)/res.rc_mac_dlg
RESINTER2 = $(SRC_PATH)/res.rc_mac_menu

$(OBJS): $(SRC_PATH)/*.h $(RESINTER) $(RESINTER2)

OBJS += $(SWELL_OBJS)

$(RESINTER): $(SRC_PATH)/res.rc
	perl WDL/swell/swell_resgen.pl --quiet $(SRC_PATH)/res.rc

$(RESINTER2): $(SRC_PATH)/res.rc $(RESINTER)

.PHONY: clean
	
$(APPNAME): $(OBJS)
	$(CXX) -o $@ -shared $(CFLAGS) $(OBJS) $(LINKEXTRA)

clean:
	-rm $(OBJS) $(APPNAME) $(RESINTER) $(RESINTER2)
