# Lanceur de tests hors-ligne pour le coeur métier.
#
# La voie normale est `pio test -e native` (c'est ce que fait la CI). Ce
# Makefile fait la même chose avec juste g++ et une copie de Unity dans
# test/support/unity, pour pouvoir tester sans accès au registre PlatformIO.
#
#   make test              compile et lance toutes les suites
#   make test-test_isa     lance une seule suite
#   make clean

CXX      ?= g++
BUILD    := build/native

CORE_SRC := $(wildcard src/core/*.cpp) $(wildcard src/ui/*.cpp)
UNITY    := test/support/unity/unity.c

CXXFLAGS := -std=gnu++17 -Wall -Wextra -Werror -O1 -g -Isrc -Itest -Itest/support/unity
# isnan()/isfinite() sont la première ligne de défense du filtre : -ffast-math
# implique -ffinite-math-only, qui autorise le compilateur à considérer ces
# tests comme toujours faux et donc à les supprimer. On l'interdit
# explicitement.
CXXFLAGS += -fno-fast-math -fno-finite-math-only

# Unity est du C compilé en C++ ici : on n'applique pas -Werror à du code tiers.
UNITY_FLAGS := -std=gnu++17 -O1 -g -Itest/support/unity -w

SUITES := $(notdir $(wildcard test/test_*))

.PHONY: test clean render golden hal-syntax bom doctor setup $(addprefix test-,$(SUITES))

test: $(addprefix test-,$(SUITES))
	@echo ""
	@echo "Toutes les suites sont passées."

$(BUILD)/unity.o: $(UNITY)
	@mkdir -p $(dir $@)
	$(CXX) $(UNITY_FLAGS) -c $< -o $@

define SUITE_RULE
$(BUILD)/$(1): $(wildcard test/$(1)/*.cpp) $(CORE_SRC) $(BUILD)/unity.o
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXXFLAGS) $(wildcard test/$(1)/*.cpp) $(CORE_SRC) $(BUILD)/unity.o -o $$@

test-$(1): $(BUILD)/$(1)
	@echo "=== $(1) ==="
	@$$<
endef

$(foreach suite,$(SUITES),$(eval $(call SUITE_RULE,$(suite))))

clean:
	rm -rf build

# Rend les écrans dans build/screens/*.png, pour vérifier la mise en page sans
# écran physique.
render: $(BUILD)/render_screens
	@mkdir -p build/screens
	@$(BUILD)/render_screens build/screens
	@python3 tools/pbm2png.py build/screens
	@echo "PNG dans build/screens/"

$(BUILD)/render_screens: tools/render_screens.cpp $(CORE_SRC)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Regénère les images de référence des tests de rendu. À ne lancer qu'après
# avoir REGARDÉ l'image obtenue et validé le changement de mise en page.
golden: $(BUILD)/make_golden
	@mkdir -p test/golden
	@$(BUILD)/make_golden test/golden

$(BUILD)/make_golden: tools/make_golden.cpp $(CORE_SRC)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Contrôle de syntaxe de la couche matérielle.
#
# Compile src/hal et src/app contre des talons Arduino/ESP-IDF minimaux (voir
# test/support/arduino_stubs/README.md). Cela attrape les fautes de frappe et les
# incohérences de types, mais NE PROUVE PAS que le firmware se compile pour la
# cible : les signatures réelles de l'IDF peuvent différer des talons. La seule
# vérification qui compte reste `pio run -e esp32s3`.
HAL_SRC := $(wildcard src/hal/*.cpp) $(wildcard src/app/*.cpp) src/main.cpp
STUBS   := test/support/arduino_stubs

hal-syntax:
	@mkdir -p $(BUILD)/hal
	@echo 'HardwareSerial Serial; TwoWire Wire;' > $(BUILD)/hal/stub_globals.cpp
	@sed -i '1i #include <Arduino.h>\n#include <Wire.h>' $(BUILD)/hal/stub_globals.cpp
	$(CXX) -std=gnu++17 -Wall -Wextra -Werror -fsyntax-only \
	    -fno-fast-math -fno-finite-math-only \
	    -Isrc -I$(STUBS) $(HAL_SRC)
	@echo "Couche materielle : syntaxe OK (ne remplace pas pio run -e esp32s3)."

# Valide les nomenclatures et produit les fichiers réduits à téléverser sur
# Octopart, TrustedParts ou FindChips.
bom:
	python3 tools/bom_check.py --emit

# Dresse l'état de l'environnement de développement.
doctor:
	@./tools/doctor.sh

# Installation guidée des outils manquants.
setup:
	@./tools/setup-linux.sh
