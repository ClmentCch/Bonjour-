#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# NXF Pro - a native Nintendo Switch homebrew file manager with an FTP server.
#
# TARGET   : output name (NXF-Pro.nro)
# BUILD    : intermediate files
# SOURCES  : C/C++ sources
# INCLUDES : headers
# ICON     : home menu icon (256x256 JPEG)
#---------------------------------------------------------------------------------
TARGET      :=  NXF-Pro
BUILD       :=  build
SOURCES     :=  src
INCLUDES    :=  include vendor

APP_TITLE   :=  NXF Pro
APP_AUTHOR  :=  ClmentCch
APP_VERSION :=  6.0.0
ICON        :=  assets/icon.jpg

#---------------------------------------------------------------------------------
# Binary assets embedded with bin2s. They are copied into $(BUILD) under a
# stable name so the generated symbols never depend on the source path.
#---------------------------------------------------------------------------------
BINARIES    :=  font_light.bin font_regular.bin font_medium.bin logo.bin

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH        :=  -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS      :=  -g -Wall -O2 -ffunction-sections -fdata-sections \
                $(ARCH)

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

export OFILES_BIN   :=  $(BINARIES:.bin=.bin.o)
export OFILES_SRC   :=  $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES       :=  $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN   :=  $(BINARIES:.bin=_bin.h)

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

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD): $(addprefix $(BUILD)/,$(BINARIES))
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
$(BUILD)/font_light.bin: assets/fonts/Montserrat-Light.ttf
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@cp $< $@

$(BUILD)/font_regular.bin: assets/fonts/Montserrat-Regular.ttf
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@cp $< $@

$(BUILD)/font_medium.bin: assets/fonts/Montserrat-Medium.ttf
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@cp $< $@

$(BUILD)/logo.bin: assets/logonxf.png
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@cp $< $@

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
