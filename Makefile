#(C)2004-2006 SourceMM Development Team
# Makefile written by David "BAILOPAN" Anderson

SMSDK = ../sourcemod-1.3
SOURCEMM = ../mmsource-1.8
HL2SDK = ../hl2sdk-ob-valve

#####################################
### EDIT BELOW FOR OTHER PROJECTS ###
#####################################

PROJECT = cssdm

OBJECTS = sdk/smsdk_ext.cpp cssdm_weapons.cpp cssdm_utils.cpp cssdm_players.cpp \
	cssdm_main.cpp cssdm_ffa.cpp cssdm_events.cpp cssdm_detours.cpp cssdm_ctrl.cpp \
	cssdm_config.cpp cssdm_callbacks.cpp 

##############################################
### CONFIGURE ANY OTHER FLAGS/OPTIONS HERE ###
##############################################

C_OPT_FLAGS = -O3 -funroll-loops -s -pipe -fno-strict-aliasing
C_DEBUG_FLAGS = -g -ggdb3
CPP_GCC4_FLAGS = -fvisibility=hidden -fvisibility-inlines-hidden
CPP = gcc

OS := $(shell uname -s)
ifeq "$(OS)" "Darwin"
	LIB_EXT = dylib
	HL2LIB = $(HL2SDK)/lib/mac
	CFLAGS += -isysroot /Developer/SDKs/MacOSX10.5.sdk
	LINK = -dynamiclib -lstdc++ -mmacosx-version-min=10.5
else
	LIB_EXT = so
	CFLAGS += -D_LINUX
	LINK = -shared
	HL2LIB = $(HL2SDK)/lib/linux
endif

HL2PUB = $(HL2SDK)/public

LINK_HL2 = $(HL2LIB)/tier1_i486.a libvstdlib.$(LIB_EXT) libtier0.$(LIB_EXT)
LINK += $(LINK_HL2) -static-libgcc

INCLUDE = -I. -I.. -Isdk -I$(HL2PUB) -I$(HL2PUB)/game/server -I$(HL2PUB)/engine -I$(HL2PUB)/tier0 \
	-I$(HL2PUB)/tier1 -I$(HL2PUB)/vstdlib -I$(HL2SDK)/tier1 -I$(SOURCEMM)/core \
	-I$(SOURCEMM)/core/sourcehook -I$(SMSDK)/public -I$(SMSDK)/public/sourcepawn -I$(SMSDK)/public/extensions \
	-I$(HL2SDK)/game/server -I$(HL2SDK)/game/shared

CFLAGS = -DNDEBUG -Dstricmp=strcasecmp -D_stricmp=strcasecmp -D_strnicmp=strncasecmp \
	 -Dstrnicmp=strncasecmp -D_snprintf=snprintf -D_vsnprintf=vsnprintf -D_alloca=alloca \
	 -Dstrcmpi=strcasecmp -Wall -Werror -Wno-switch -Wno-unused -Wno-invalid-offsetof -fPIC \
	 -msse -DSOURCEMOD_BUILD -DHAVE_STDINT_H -Wno-uninitialized -m32
CPPFLAGS = -Wno-non-virtual-dtor -fno-exceptions -fno-rtti

################################################
### DO NOT EDIT BELOW HERE FOR MOST PROJECTS ###
################################################

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug
	CFLAGS += $(C_DEBUG_FLAGS)
else
	BIN_DIR = Release
	CFLAGS += $(C_OPT_FLAGS)
endif


GCC_VERSION := $(shell $(CPP) -dumpversion >&1 | cut -b1)
ifeq "$(GCC_VERSION)" "4"
	CPPFLAGS += $(CPP_GCC4_FLAGS)
endif

BINARY = $(PROJECT).ext.$(LIB_EXT)

OBJ_LINUX := $(OBJECTS:%.cpp=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.cpp
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

all:
	mkdir -p $(BIN_DIR)/sdk
	ln -sf $(HL2LIB)/libvstdlib.$(LIB_EXT) libvstdlib.$(LIB_EXT)
	ln -sf $(HL2LIB)/libtier0.$(LIB_EXT) libtier0.$(LIB_EXT)
	$(MAKE) extension

extension: $(OBJ_LINUX)
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) $(OBJ_LINUX) $(LINK) -shared -ldl -lm -o $(BIN_DIR)/$(BINARY)

debug:	
	$(MAKE) all DEBUG=true

default: all

clean:
	rm -rf Release/*.o
	rm -rf Release/sdk/*.o
	rm -rf Release/$(BINARY)
	rm -rf Debug/*.o
	rm -rf Debug/sdk/*.o
	rm -rf Debug/$(BINARY)
