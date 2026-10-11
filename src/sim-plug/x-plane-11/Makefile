PLUGIN  := lltracker
SDK     := plug-sdk
SDK_URL := https://developer.x-plane.com/wp-content/plugins/code-sample-generation/sdk_zip_files/XPSDK213.zip
SRC     := plug.c

HOST    := $(shell uname -s)

INC     := -I$(SDK)/CHeaders/XPLM -I$(SDK)/CHeaders/Widgets
CFLAGS  := -Wall -Wextra -O2 -DXPLM200=1 -DXPLM210=1 $(INC)

CURL_WIN_URL  := https://curl.se/windows/latest.cgi?p=win64-mingw.zip
CURL_WIN_DIR  := third_party/curl-win
CURL_WIN_HDR  := $(CURL_WIN_DIR)/include/curl/curl.h
CURL_WIN_LIB  := $(CURL_WIN_DIR)/lib/libcurl.dll.a
CURL_WIN_DLL  := $(CURL_WIN_DIR)/bin/libcurl-x64.dll

LIN_CURL_FLAGS := -lcurl
WIN_CURL_FLAGS := -I$(CURL_WIN_DIR)/include
MAC_CURL_FLAGS := -I$(CURL_WIN_DIR)/include

LIN_OUT := $(PLUGIN)/64/lin.xpl
WIN_OUT := $(PLUGIN)/64/win.xpl
MAC_OUT := $(PLUGIN)/64/mac.xpl

LIN_CC      := gcc
LIN_CFLAGS  := $(CFLAGS) -fPIC -fvisibility=hidden -m64 -DLIN=1 -DAPL=0 -DIBM=0
LIN_LDFLAGS := -shared -rdynamic -nodefaultlibs $(LIN_CURL_FLAGS)

WIN_CC      := zig cc
WIN_CFLAGS  := $(CFLAGS) $(WIN_CURL_FLAGS) -DIBM=1 -DAPL=0 -DLIN=0
WIN_LDFLAGS := -shared -target x86_64-windows-gnu -lbcrypt $(CURL_WIN_LIB) $(SDK)/Libraries/Win/XPLM_64.lib $(SDK)/Libraries/Win/XPWidgets_64.lib

MAC_CC      := zig cc
MAC_CFLAGS  := $(CFLAGS) $(MAC_CURL_FLAGS) -fPIC -fvisibility=hidden -DAPL=1 -DIBM=0 -DLIN=0
MAC_LDFLAGS := -dynamiclib -target x86_64-macos -undefined dynamic_lookup

MAC_HDR_DEP := $(CURL_WIN_HDR)

ifeq ($(HOST),Darwin)
MAC_CC      := clang
MAC_CFLAGS  := $(CFLAGS) -fPIC -fvisibility=hidden -DAPL=1 -DIBM=0 -DLIN=0
MAC_LDFLAGS := -bundle -undefined dynamic_lookup
MAC_HDR_DEP :=
else ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(HOST)))
WIN_CC      := gcc
WIN_LDFLAGS := -shared -lbcrypt $(CURL_WIN_LIB) $(SDK)/Libraries/Win/XPLM_64.lib $(SDK)/Libraries/Win/XPWidgets_64.lib
endif

XPINSTD ?= $(HOME)/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/X-Plane 11
XPDEST  := $(XPINSTD)/Resources/plugins/$(PLUGIN)/64

.PHONY: all host linux windows mac clean distclean install

all: linux windows mac

host:
ifeq ($(HOST),Darwin)
	$(MAKE) mac
else ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(HOST)))
	$(MAKE) windows
else
	$(MAKE) linux
endif

linux: $(LIN_OUT)
windows: $(WIN_OUT)
mac: $(MAC_OUT)

$(LIN_OUT): $(SRC) | $(SDK)/CHeaders
	@mkdir -p $(dir $@)
	$(LIN_CC) $(LIN_CFLAGS) $< -o $@ $(LIN_LDFLAGS)

$(WIN_OUT): $(SRC) $(CURL_WIN_HDR) | $(SDK)/CHeaders
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) $< -o $@ $(WIN_LDFLAGS)
	@rm -f $(dir $@)*.pdb $(dir $@)$(basename $(notdir $(SRC))).lib
	@cp $(CURL_WIN_DLL) $(dir $@)

$(MAC_OUT): $(SRC) $(MAC_HDR_DEP) | $(SDK)/CHeaders
	@mkdir -p $(dir $@)
	$(MAC_CC) $(MAC_CFLAGS) $< -o $@ $(MAC_LDFLAGS)

$(CURL_WIN_HDR):
	@mkdir -p $(CURL_WIN_DIR)
	curl -fL -o $(CURL_WIN_DIR).zip "$(CURL_WIN_URL)"
	rm -rf $(CURL_WIN_DIR).tmp && mkdir -p $(CURL_WIN_DIR).tmp
	unzip -q $(CURL_WIN_DIR).zip -d $(CURL_WIN_DIR).tmp
	rm -rf $(CURL_WIN_DIR) && mv $(CURL_WIN_DIR).tmp/*/ $(CURL_WIN_DIR)
	rm -rf $(CURL_WIN_DIR).tmp $(CURL_WIN_DIR).zip

$(SDK)/CHeaders:
	curl -fL -o $(SDK).zip $(SDK_URL)
	rm -rf $(SDK).tmp && mkdir -p $(SDK).tmp
	unzip -q $(SDK).zip -d $(SDK).tmp
	rm -rf $(SDK) && mv $(SDK).tmp/SDK $(SDK)
	rm -rf $(SDK).tmp $(SDK).zip

clean:
	rm -rf $(PLUGIN)

distclean: clean
	rm -rf $(SDK) $(SDK).tmp $(SDK).zip $(CURL_WIN_DIR) $(CURL_WIN_DIR).tmp $(CURL_WIN_DIR).zip

install: all
	@mkdir -p "$(XPDEST)"
	cp $(LIN_OUT) $(WIN_OUT) $(MAC_OUT) "$(XPDEST)/"
	cp $(CURL_WIN_DLL) "$(XPDEST)/"
