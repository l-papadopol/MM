# -----------------------------------------------------------------------------
# MadModem convenience Makefile
#
# v1.08 all-in-one policy: Hamlib is compiled from the bundled source before
# MadModem is configured.  The default build links real Hamlib CAT/PTT support;
# no silent Hamlib stub is used unless you explicitly pass
# -DMADMODEM_REQUIRE_HAMLIB=OFF yourself.
# -----------------------------------------------------------------------------

APP        := MadModem
BUILD_TYPE ?= Release
JOBS       ?= $(shell nproc 2>/dev/null || echo 4)

MXE_ROOT   ?= $(if $(wildcard /home/iz6nnh/mxe),/home/iz6nnh/mxe,$(HOME)/mxe)
MXE_TARGET ?= x86_64-w64-mingw32.static
MXE_CMAKE  := $(MXE_ROOT)/usr/bin/$(MXE_TARGET)-cmake

HAMLIB_LINUX_ROOT ?= $(CURDIR)/third_party/hamlib_lgpl/install-linux-x86_64
HAMLIB_WIN_ROOT   ?= $(CURDIR)/third_party/hamlib_lgpl/install-win64-static

.PHONY: all linux windows win64-static macos package-macos hamlib-linux hamlib-windows run clean distclean help

all: linux

hamlib-linux:
	HAMLIB_PREFIX="$(HAMLIB_LINUX_ROOT)" HAMLIB_STATIC=on HAMLIB_SHARED=off JOBS=$(JOBS) \
		bash "$(CURDIR)/third_party/hamlib_lgpl/build_hamlib.sh"

linux: hamlib-linux
	cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DHAMLIB_ROOT="$(HAMLIB_LINUX_ROOT)" -DMADMODEM_REQUIRE_HAMLIB=ON
	cmake --build build-linux -j$(JOBS)

windows: win64-static

macos:
	bash "$(CURDIR)/scripts/build_macos.sh"

package-macos: macos
	bash "$(CURDIR)/scripts/package_macos.sh"

hamlib-windows:
	@test -x "$(MXE_CMAKE)" || { \
		echo "ERROR: MXE CMake wrapper not found: $(MXE_CMAKE)"; \
		echo "Set MXE_ROOT=/path/to/mxe or MXE_TARGET=x86_64-w64-mingw32.static"; \
		exit 1; \
	}
	MXE_ROOT="$(MXE_ROOT)" MXE_TARGET="$(MXE_TARGET)" HAMLIB_PREFIX="$(HAMLIB_WIN_ROOT)" JOBS=$(JOBS) \
		bash "$(CURDIR)/third_party/hamlib_lgpl/build_hamlib_mxe.sh"

win64-static: hamlib-windows
	PATH="$(MXE_ROOT)/usr/bin:$$PATH" "$(MXE_CMAKE)" -S . -B build-win64-static \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DHAMLIB_ROOT="$(HAMLIB_WIN_ROOT)" \
		-DPKG_CONFIG_EXECUTABLE="$(MXE_ROOT)/usr/bin/$(MXE_TARGET)-pkg-config" \
		-DMADMODEM_REQUIRE_HAMLIB=ON
	cmake --build build-win64-static -j$(JOBS)

run: linux
	./build-linux/$(APP)

clean:
	cmake --build build-linux --target clean 2>/dev/null || true
	cmake --build build-win64-static --target clean 2>/dev/null || true
	cmake --build build-macos-$$(uname -m) --target clean 2>/dev/null || true

# Keep Hamlib install prefixes by default. Remove them only with distclean.
distclean:
	rm -rf build-linux build-win64-static build-macos-* dist \
		third_party/hamlib_lgpl/build-* \
		third_party/hamlib_lgpl/install-linux-x86_64 \
		third_party/hamlib_lgpl/install-win64-static

help:
	@echo "MadModem build targets:"
	@echo "  make linux          Build bundled Hamlib + native Linux binary"
	@echo "  make win64-static   Build bundled Hamlib + Windows static EXE using MXE"
	@echo "  make windows        Alias of win64-static"
	@echo "  make macos          Build native macOS .app bundle on a Mac/Homebrew host"
	@echo "  make package-macos  Build and package unsigned macOS ZIP/DMG"
	@echo "  make run            Build and run Linux binary"
	@echo "  make clean          Clean MadModem build trees"
	@echo "  make distclean      Remove MadModem and Hamlib build/install trees"
	@echo ""
	@echo "Quick all-in-one Linux + Windows build: ./build_all.sh (or: bash build_all.sh if your extractor stripped executable bits)"
	@echo "macOS CI build: GitHub Actions -> Build macOS unsigned"
	@echo "Variables: BUILD_TYPE=$(BUILD_TYPE), JOBS=$(JOBS), MXE_ROOT=$(MXE_ROOT), MXE_TARGET=$(MXE_TARGET)"
