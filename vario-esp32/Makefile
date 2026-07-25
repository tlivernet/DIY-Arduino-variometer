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

CORE_SRC := $(wildcard src/core/*.cpp)
UNITY    := test/support/unity/unity.c

CXXFLAGS := -std=gnu++17 -Wall -Wextra -Werror -O1 -g -Isrc -Itest/support/unity
# isnan()/isfinite() sont la première ligne de défense du filtre : -ffast-math
# implique -ffinite-math-only, qui autorise le compilateur à considérer ces
# tests comme toujours faux et donc à les supprimer. On l'interdit
# explicitement.
CXXFLAGS += -fno-fast-math -fno-finite-math-only

# Unity est du C compilé en C++ ici : on n'applique pas -Werror à du code tiers.
UNITY_FLAGS := -std=gnu++17 -O1 -g -Itest/support/unity -w

SUITES := $(notdir $(wildcard test/test_*))

.PHONY: test clean $(addprefix test-,$(SUITES))

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
