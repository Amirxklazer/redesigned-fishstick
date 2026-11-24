.RECIPEPREFIX = >
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO")
endif
TARGET   := ilot14_hollow
PORTLIBS := $(DEVKITPRO)/portlibs/switch
CXX      := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++
ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CXXFLAGS := -g -Wall -O2 -ffunction-sections $(ARCH) -D__SWITCH__ -std=gnu++17 \
            -I$(DEVKITPRO)/libnx/include -I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2 -Isrc
LDFLAGS  := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -L$(DEVKITPRO)/libnx/lib -L$(PORTLIBS)/lib
LIBS     := -lSDL2_ttf -lfreetype -lpng -lbz2 -lz $(shell $(PORTLIBS)/bin/sdl2-config --libs) -lnx -lm

all: $(TARGET).nro

$(TARGET).elf: src/main.cpp src/zones_base.h
> $(CXX) $(CXXFLAGS) src/main.cpp $(LDFLAGS) $(LIBS) -o $@

$(TARGET).nacp:
> nacptool --create "Îlot14: Hollow Ashes" "14" "0.1.0" $@

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp icon.jpg
> elf2nro $(TARGET).elf $@ --icon=icon.jpg --nacp=$(TARGET).nacp

clean:
> rm -f $(TARGET).elf $(TARGET).nacp $(TARGET).nro
