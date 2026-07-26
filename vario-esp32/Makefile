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

.PHONY: test clean render golden replay hal-syntax bom case schematic erc pcb route fab doctor setup $(addprefix test-,$(SUITES))

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

# Harnais de rejeu : rejoue un enregistrement de trames brutes à travers toute
# la chaîne d'estimation, en natif.
#
#   make replay TRACE=traces/vol.csv
#
# Produit la trace estimée sur la sortie standard (altitude, vario, son). Le
# fichier d'entrée s'obtient avec tools/record.py branché sur le firmware.
replay: $(BUILD)/replay
	@test -n "$(TRACE)" || { echo "Usage : make replay TRACE=chemin/vers/trace.csv"; exit 2; }
	@$(BUILD)/replay "$(TRACE)"

$(BUILD)/replay: tools/replay.cpp $(CORE_SRC)
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

# Exporte les deux coquilles du boîtier (STL) et un aperçu (PNG) dans
# build/case/. Le modèle paramétrique est hardware/case/boitier.scad.
# Nécessite openscad (voir make doctor).
CASE_SRC := hardware/case/boitier.scad
case:
	@mkdir -p build/case
	@command -v openscad >/dev/null || { echo "openscad introuvable (make setup)"; exit 2; }
	openscad -o build/case/boitier-fond.stl  -D 'piece="fond"'  $(CASE_SRC)
	openscad -o build/case/boitier-capot.stl -D 'piece="capot"' $(CASE_SRC)
	openscad -o build/case/apercu.png --render --imgsize=1000,760 \
	    --camera=0,0,0,60,0,20,360 -D 'piece="apercu"' $(CASE_SRC)
	@echo "STL et aperçu dans build/case/"

# Engendre le schéma KiCad depuis la table de connexions (étape P7).
# Le schéma est régénérable : sa source de vérité est generate_schematic.py.
schematic:
	@command -v python3 >/dev/null || { echo "python3 introuvable"; exit 2; }
	cd hardware/pcb && python3 generate_schematic.py

# Contrôle électrique du schéma (ERC). Nécessite kicad-cli (KiCad 9).
# Trois catégories d'avertissements sont attendues et filtrées, car elles ne
# traduisent pas un défaut de conception :
#   - lib_symbol_issues / footprint_link : bibliothèques non déclarées en mode
#     headless (kicad-cli n'a pas les tables de l'utilisateur) ;
#   - lib_symbol_mismatch : les symboles à héritage (MS5611, TLV75733) sont
#     APLATIS par le générateur — leur copie embarquée diffère structurellement
#     de la version « extends » en bibliothèque, sans conséquence électrique.
# On ne compte comme échec que les autres catégories.
erc:
	@command -v kicad-cli >/dev/null || { echo "kicad-cli introuvable (installer KiCad 9)"; exit 2; }
	@kicad-cli sch erc hardware/pcb/vario.kicad_sch --severity-all -o /tmp/vario-erc.rpt >/dev/null 2>&1 || true
	@reste=$$(grep -vE 'lib_symbol_issues|lib_symbol_mismatch|footprint_link' /tmp/vario-erc.rpt | grep -cE '^\[' || true); \
	 echo "Violations de conception (hors bruit de config) : $$reste"; \
	 test "$$reste" -eq 0 || { echo "ERC non propre :"; grep -vE 'lib_symbol_issues|footprint_link' /tmp/vario-erc.rpt | grep -E '^\[' ; exit 1; }
	@echo "ERC propre."

# Engendre le circuit imprimé placé (empreintes + contour + plans de masse +
# vias de couture) depuis la netlist du schéma. Ne route pas.
pcb:
	@command -v kicad-cli >/dev/null || { echo "kicad-cli introuvable"; exit 2; }
	kicad-cli sch export netlist hardware/pcb/vario.kicad_sch -o /tmp/vario.net
	cd hardware/pcb && python3 generate_pcb.py

# Route le PCB avec freerouting puis réimporte. Nécessite freerouting.jar
# (55 Mo, non versionné) : le télécharger depuis
# https://github.com/freerouting/freerouting/releases et le passer via JAR=.
#   make route JAR=/chemin/freerouting.jar
route:
	@test -n "$(JAR)" || { echo "Usage : make route JAR=/chemin/freerouting.jar"; exit 2; }
	cd hardware/pcb && python3 route_pcb.py --jar "$(JAR)" --passes 200

# Exporte les fichiers de fabrication JLCPCB (Gerbers, perçage, position, BOM)
# dans build/fab/ et une archive build/vario-gerbers.zip.
fab:
	cd hardware/pcb && python3 export_fab.py

# Dresse l'état de l'environnement de développement.
doctor:
	@./tools/doctor.sh

# Installation guidée des outils manquants.
setup:
	@./tools/setup-linux.sh
