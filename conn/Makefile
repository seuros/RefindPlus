# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
#
# Conn engine -- host build. The SAME core/ + efi/ sources also compile into
# Meridian's EDK2 PE32+ image (see PORTING when that target lands); only host/
# is host-specific. The engine is built against the real MdePkg headers so
# there is zero type drift from firmware.

CC      ?= cc
EDK2    ?= ../edk2
MDE     := $(EDK2)/MdePkg/Include

# Real UEFI headers; -ffreestanding + -fshort-wchar match the firmware ABI.
# EFIAPI (ms_abi on X64) is provided by ProcessorBind.h -- do not override it.
# -DMDE_CPU_X64= (empty) matches ProcessorBind.h's own empty define; a bare
# -DMDE_CPU_X64 expands to 1 and trips -Wmacro-redefined.
EFI_INC  := -I$(MDE) -I$(MDE)/X64 -DMDE_CPU_X64=
CFLAGS   ?= -O2 -std=c11 -ffreestanding -fshort-wchar -Wall -Wextra \
            -Wno-unused-parameter $(EFI_INC) -Icore -Iefi -Ihost \
            -I../libstatemachines/include
BUILD    := build

STATE_MACHINE := ../libstatemachines/src/state_machine.c
CORE     := core/paint.c core/grid.c core/widget_clock.c core/widget_editline.c core/widget_list.c core/widget_panel.c core/text_gop.c core/ui.c core/app.c $(STATE_MACHINE) core/conn_run.c
HOST_PNG := host/shim.c host/main_png.c host/png.c host/scan_mock.c

PNGBIN   := $(BUILD)/conn-host-png
GOLD     := test/golden

# ImageMagick 7 puts everything behind `magick`; ImageMagick 6, which is still
# what most CI images ship, only has the bare tools. Accept either.
MAGICK   := $(shell command -v magick 2>/dev/null)
COMPARE  := $(if $(MAGICK),$(MAGICK) compare,compare)

.PHONY: all clean render font splash test golden smoke uitest editlinetest listtest paneltest

all: $(PNGBIN)

# deterministic golden-image path: real engine -> fake GOP -> PNG (no SDL)
$(PNGBIN): $(CORE) $(HOST_PNG) core/conn_core.h efi/efi_env.h core/font_atlas.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(CORE) $(HOST_PNG)

render: $(PNGBIN)
	$(PNGBIN) $(BUILD)/conn-text-crt-80x25.png

# one render per theme, quantised for the README. Needs ImageMagick.
MEDIA := ../.github/media
render-themes: $(PNGBIN)
	@mkdir -p $(MEDIA)
	@set -e; i=0; for n in atari pdp c64; do \
	  $(PNGBIN) $(BUILD)/theme-$$n.png $$i >/dev/null; \
	  $(MAGICK) $(BUILD)/theme-$$n.png -strip -colors 64 PNG8:$(MEDIA)/conn-$$n.png; \
	  echo "  $(MEDIA)/conn-$$n.png"; i=$$((i+1)); \
	done

# regression test: engine output must be pixel-identical to the committed golden
# (needs ImageMagick). Update the golden with `make golden` after a deliberate
# visual change.
# pure-core unit tests: update boundary + scroll/selection (no firmware, no Gop)
uitest: | $(BUILD)
	@$(CC) $(CFLAGS) -o $(BUILD)/conn-uitest core/paint.c core/grid.c core/widget_clock.c core/ui.c core/app.c $(STATE_MACHINE) test/ui_test.c
	@$(BUILD)/conn-uitest

editlinetest: | $(BUILD)
	@$(CC) $(CFLAGS) -o $(BUILD)/conn-editlinetest core/paint.c core/grid.c core/widget_clock.c core/widget_editline.c $(STATE_MACHINE) test/editline_test.c
	@$(BUILD)/conn-editlinetest

listtest: | $(BUILD)
	@$(CC) $(CFLAGS) -o $(BUILD)/conn-listtest core/paint.c core/grid.c core/widget_clock.c core/widget_list.c $(STATE_MACHINE) test/list_test.c
	@$(BUILD)/conn-listtest

paneltest: | $(BUILD)
	@$(CC) $(CFLAGS) -o $(BUILD)/conn-paneltest core/paint.c core/grid.c core/widget_clock.c core/widget_panel.c $(STATE_MACHINE) test/panel_test.c
	@$(BUILD)/conn-paneltest

test: $(PNGBIN) uitest editlinetest listtest paneltest
	@$(PNGBIN) $(BUILD)/_test-crt.png >/dev/null
	@if $(COMPARE) -metric AE $(BUILD)/_test-crt.png $(GOLD)/text-crt.png null: 2>/dev/null; then \
	   echo "PASS  text-crt  (pixel-identical to golden)"; \
	 else echo; echo "FAIL  text-crt  (differs from golden)"; \
	   echo "  inspect: $(COMPARE) $(BUILD)/_test-crt.png $(GOLD)/text-crt.png /tmp/conn-diff.png"; \
	   exit 1; fi

golden: $(PNGBIN)
	@$(PNGBIN) $(GOLD)/text-crt.png >/dev/null && echo "updated $(GOLD)/text-crt.png"

# firmware smoke: build the PE32+ app, boot it under QEMU+OVMF, screendump to PNG
smoke:
	test/qemu-smoke.sh $(BUILD)/conn-qemu.png

font:
	python3 tools/bake_font.py

# Bake the per-OS boot-splash logos (conn/art/splash/*.svg) into the firmware
# header core/splash_atlas.h, consumed by ui/conn_bridge.c (ConnLaunchSplash).
# Offline only: needs rsvg-convert + Pillow, never run at boot.
splash:
	python3 tools/bake_splash.py

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
