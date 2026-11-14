#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Bonjour! - a native Nintendo Switch homebrew application.
#
# TARGET   : output name (Bonjour.nro)
# BUILD    : intermediate files
# SOURCES  : C/C++ sources
# INCLUDES : headers
# ICON     : home menu icon (256x256 JPEG)
#---------------------------------------------------------------------------------
TARGET      :=  Bonjour
BUILD       :=  build
SOURCES     :=  src
INCLUDES    :=  include vendor

APP_TITLE   :=  Bonjour!
APP_AUTHOR  :=  ClmentCch
APP_VERSION :=  1.0.0
ICON        :=  assets/icon.jpg

#---------------------------------------------------------------------------------
# Font selection.
#
# Times New Roman is proprietary and must not be redistributed, so it is not
# committed. If a legally obtained copy is placed at assets/fonts/times.ttf it is
# embedded instead of the bundled font. Otherwise the build embeds Liberation
# Serif, which is metric-compatible with Times New Roman and
# redistributable under the SIL Open Font License.
#---------------------------------------------------------------------------------
TIMES_TTF    := assets/fonts/times.ttf
FALLBACK_TTF := assets/fonts/LiberationSerif-Regular.ttf

ifneq ($(wildcard $(TIMES_TTF)),)
FONT_SRC      := $(TIMES_TTF)
FONT_IS_TIMES := 1
else
FONT_SRC      := $(FALLBACK_TTF)
FONT_IS_TIMES := 0
endif

DEFINES     :=  -DBONJOUR_FONT_IS_TIMES=$(FONT_IS_TIMES)

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH        :=  -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS      :=  -g -Wall -O2 -ffunction-sections -fdata-sections \
                $(ARCH) $(DEFINES)

CFLAGS      +=  $(INCLUDE) -D__SWITCH__

CXXFLAGS    :=  $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions

ASFLAGS     :=  -g $(ARCH)
LDFLAGS      =  -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS        :=  -lnx

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS     :=  $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   :=  $(CURDIR)/$(TARGET)
export TOPDIR   :=  $(CURDIR)

export VPATH    :=  $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

export DEPSDIR  :=  $(CURDIR)/$(BUILD)

CFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
    export LD   :=  $(CC)
else
    export LD   :=  $(CXX)
endif
#---------------------------------------------------------------------------------

# font.bin.o comes from the TrueType file copied into $(BUILD) as font.bin
export OFILES_BIN   :=  font.bin.o
export OFILES_SRC   :=  $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES       :=  $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN   :=  font_bin.h

export INCLUDE  :=  $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                    $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                    -I$(CURDIR)/$(BUILD)

export LIBPATHS :=  $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
    icons := $(wildcard *.jpg)
    ifneq (,$(findstring $(TARGET).jpg,$(icons)))
        export APP_ICON := $(TOPDIR)/$(TARGET).jpg
    else
        ifneq (,$(findstring icon.jpg,$(icons)))
            export APP_ICON := $(TOPDIR)/icon.jpg
        endif
    endif
else
    export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
    export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
    export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
    export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD): $(BUILD)/font.bin
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
# The font is copied under a stable name so bin2s always produces the same
# symbols (font_bin / font_bin_size) whatever file was selected above.
#---------------------------------------------------------------------------------
$(BUILD)/font.bin: $(FONT_SRC)
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@cp $< $@
	@echo "font: embedding $(notdir $<)"

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all clean

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all : $(OUTPUT).nro

$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp

$(OUTPUT).elf : $(OFILES)

$(OFILES_SRC) : $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
