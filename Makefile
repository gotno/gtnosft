SHELL:=/bin/bash -O extglob

RACK_DIR ?= ../..

#FLAGS += -w
	
# Add .cpp and .c files to the build
SOURCES = \
		$(wildcard dep/oscpack/ip/*.cpp) \
		$(wildcard dep/oscpack/osc/*.cpp) \
		$(wildcard src/*.cpp) \
		$(wildcard src/*/*.cpp) \

# Careful about linking to libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine.
include $(RACK_DIR)/arch.mk

MACHINE = $(shell $(CC) -dumpmachine)
ifneq (, $(findstring mingw, $(MACHINE)))
	SOURCES += $(wildcard dep/oscpack/ip/win32/*.cpp) 
	LDFLAGS += -lws2_32 -lwinmm
	LDFLAGS +=  -L$(RACK_DIR)/dep/lib
else
	SOURCES += $(wildcard dep/oscpack/ip/posix/*.cpp) 
endif


DISTRIBUTABLES += $(wildcard LICENSE*) \
 dep/oscpack/LICENSE \
 $(wildcard res/*.svg) \
 $(wildcard res/*/*.svg) 

include $(RACK_DIR)/plugin.mk
