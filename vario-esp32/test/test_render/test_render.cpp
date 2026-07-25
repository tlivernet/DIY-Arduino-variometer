#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "support/reference_states.h"
#include "ui/canvas.h"
#include "ui/screens.h"
#include "ui/segment.h"

void setUp() {}
void tearDown() {}

namespace {

using vario::ui::Canvas;
using vario::ui::kScreenHeight;
using vario::ui::kScreenWidth;

std::vector<std::uint8_t> makeBuffer() {
  return std::vector<std::uint8_t>(
      static_cast<std::size_t>((kScreenWidth + 7) / 8) * kScreenHeight, 0);
}

// Compare le rendu à une image de référence versionnée.
//
// Les écrans étant du C++ pur, une régression de mise en page se voit ici sans
// matériel. En cas d'écart, l'image obtenue est écrite dans build/ pour pouvoir
// être ouverte et comparée à l'œil.
void expectMatchesGolden(const Canvas& canvas, const char* name) {
  const std::string golden_path = std::string("test/golden/") + name + ".pbm";
  const std::size_t bytes = static_cast<std::size_t>(canvas.stride()) * canvas.height();

  std::FILE* file = std::fopen(golden_path.c_str(), "rb");
  if (file == nullptr) {
    TEST_FAIL_MESSAGE("image de reference absente (lancer make golden)");
    return;
  }

  // On saute l'en-tête « P4\n<w> <h>\n ».
  char header[64];
  if (std::fgets(header, sizeof(header), file) == nullptr ||
      std::fgets(header, sizeof(header), file) == nullptr) {
    std::fclose(file);
    TEST_FAIL_MESSAGE("en-tete de reference illisible");
    return;
  }

  std::vector<std::uint8_t> golden(bytes, 0);
  const std::size_t read = std::fread(golden.data(), 1, bytes, file);
  std::fclose(file);

  if (read != bytes) {
    TEST_FAIL_MESSAGE("taille de reference inattendue");
    return;
  }

  if (std::memcmp(golden.data(), canvas.buffer(), bytes) != 0) {
    const std::string actual = std::string("build/") + name + "-obtenu.pbm";
    if (std::FILE* out = std::fopen(actual.c_str(), "wb")) {
      std::fprintf(out, "P4\n%d %d\n", canvas.width(), canvas.height());
      std::fwrite(canvas.buffer(), 1, bytes, out);
      std::fclose(out);
    }
    TEST_FAIL_MESSAGE("le rendu differe de l'image de reference");
  }
}

}  // namespace

// --- Primitives -------------------------------------------------------------

static void test_canvas_starts_blank() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  TEST_ASSERT_EQUAL_INT(kScreenWidth, canvas.width());
  TEST_ASSERT_EQUAL_INT(kScreenHeight, canvas.height());
  TEST_ASSERT_EQUAL_INT(50, canvas.stride());
  TEST_ASSERT_FALSE(canvas.pixel(0, 0));
  TEST_ASSERT_FALSE(canvas.pixel(399, 239));
}

static void test_set_and_read_pixel() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  canvas.setPixel(7, 3, true);
  TEST_ASSERT_TRUE(canvas.pixel(7, 3));
  TEST_ASSERT_FALSE(canvas.pixel(8, 3));
  TEST_ASSERT_FALSE(canvas.pixel(7, 4));

  canvas.setPixel(7, 3, false);
  TEST_ASSERT_FALSE(canvas.pixel(7, 3));
}

static void test_out_of_bounds_is_ignored() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  // Ne doit ni écrire hors du tampon ni planter.
  canvas.setPixel(-1, 0, true);
  canvas.setPixel(0, -1, true);
  canvas.setPixel(kScreenWidth, 0, true);
  canvas.setPixel(0, kScreenHeight, true);
  canvas.fillRect(-50, -50, 20, 20, true);
  canvas.fillRect(kScreenWidth + 10, 0, 20, 20, true);

  TEST_ASSERT_EQUAL_INT(0, canvas.dirtyLineCount());
}

static void test_fill_and_draw_rect() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  canvas.fillRect(10, 20, 5, 4, true);
  TEST_ASSERT_TRUE(canvas.pixel(10, 20));
  TEST_ASSERT_TRUE(canvas.pixel(14, 23));
  TEST_ASSERT_FALSE(canvas.pixel(15, 20));
  TEST_ASSERT_FALSE(canvas.pixel(10, 24));

  canvas.clear(false);
  canvas.drawRect(50, 60, 10, 8, true);
  TEST_ASSERT_TRUE(canvas.pixel(50, 60));   // coin
  TEST_ASSERT_TRUE(canvas.pixel(59, 67));   // coin opposé
  TEST_ASSERT_FALSE(canvas.pixel(55, 63));  // creux
}

static void test_draw_line_endpoints_and_diagonal() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  canvas.drawLine(10, 10, 20, 20, true);
  TEST_ASSERT_TRUE(canvas.pixel(10, 10));
  TEST_ASSERT_TRUE(canvas.pixel(15, 15));
  TEST_ASSERT_TRUE(canvas.pixel(20, 20));

  canvas.clear(false);
  canvas.drawLine(30, 40, 30, 50, true);  // verticale
  TEST_ASSERT_TRUE(canvas.pixel(30, 45));
  TEST_ASSERT_FALSE(canvas.pixel(31, 45));
}

static void test_text_width_and_rendering() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  // 5 colonnes par glyphe + 1 d'avance, sans avance finale.
  TEST_ASSERT_EQUAL_INT(5, Canvas::textWidth("A", 1));
  TEST_ASSERT_EQUAL_INT(11, Canvas::textWidth("AB", 1));
  TEST_ASSERT_EQUAL_INT(22, Canvas::textWidth("AB", 2));
  TEST_ASSERT_EQUAL_INT(0, Canvas::textWidth("", 1));
  TEST_ASSERT_EQUAL_INT(0, Canvas::textWidth(nullptr, 1));

  canvas.drawText(0, 0, "A", true, 1);
  TEST_ASSERT_TRUE(canvas.dirtyLineCount() > 0);
}

// --- Suivi des lignes modifiées ---------------------------------------------
//
// Ce n'est pas une optimisation : une trame complète du Sharp LS027B7DH01 coûte
// 12,5 ko à 1 MHz, soit ~100 ms. Sans ce suivi, un rafraîchissement à 10 Hz est
// impossible.

static void test_clear_marks_every_line_dirty() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  canvas.clearDirty();
  TEST_ASSERT_EQUAL_INT(0, canvas.dirtyLineCount());

  canvas.clear(false);
  TEST_ASSERT_EQUAL_INT(kScreenHeight, canvas.dirtyLineCount());
}

static void test_only_touched_lines_are_dirty() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);
  canvas.clearDirty();

  canvas.fillRect(100, 50, 20, 10, true);

  TEST_ASSERT_EQUAL_INT(10, canvas.dirtyLineCount());
  TEST_ASSERT_TRUE(canvas.lineDirty(50));
  TEST_ASSERT_TRUE(canvas.lineDirty(59));
  TEST_ASSERT_FALSE(canvas.lineDirty(49));
  TEST_ASSERT_FALSE(canvas.lineDirty(60));

  int first = -1;
  int last = -1;
  canvas.dirtyRange(first, last);
  TEST_ASSERT_EQUAL_INT(50, first);
  TEST_ASSERT_EQUAL_INT(59, last);
}

// Écrire la valeur déjà présente ne doit PAS salir la ligne, sinon on
// retransmettrait tout l'écran à chaque image et le suivi ne servirait à rien.
static void test_redundant_write_does_not_dirty() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  canvas.fillRect(0, 100, 50, 5, true);
  canvas.clearDirty();

  canvas.fillRect(0, 100, 50, 5, true);  // exactement la même chose
  TEST_ASSERT_EQUAL_INT(0, canvas.dirtyLineCount());

  canvas.fillRect(0, 100, 50, 5, false);  // là il y a un vrai changement
  TEST_ASSERT_EQUAL_INT(5, canvas.dirtyLineCount());
}

// Cas d'usage réel : seule l'heure change d'une seconde à l'autre. Le nombre de
// lignes à retransmettre doit rester une petite fraction de l'écran.
static void test_partial_update_touches_few_lines() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  vario::ui::DisplayState state = reference::varioState();
  vario::ui::drawVarioScreen(canvas, state);
  canvas.clearDirty();

  // Une seconde plus tard, rien d'autre n'a bougé.
  state.second = 8;
  vario::ui::drawVarioScreen(canvas, state);

  // drawVarioScreen efface tout, donc tout est sale : c'est attendu. Ce test
  // documente la conséquence — le rendu complet doit rester réservé aux
  // changements d'écran, et l'appelant doit redessiner par zones pour tirer
  // parti du suivi.
  TEST_ASSERT_EQUAL_INT(kScreenHeight, canvas.dirtyLineCount());

  // En revanche, ne redessiner que le champ horloge ne salit qu'une poignée de
  // lignes : c'est ce que fera la tâche d'affichage.
  canvas.clearDirty();
  canvas.fillRect(270, 82, 100, 14, false);
  canvas.drawText(270, 82, "13:42:09", true, 2);
  TEST_ASSERT_TRUE(canvas.dirtyLineCount() <= 20);
}

// --- Sept segments ----------------------------------------------------------

static void test_segment_width_is_consistent() {
  vario::ui::SegmentMetrics m;
  m.width = 58;
  m.height = 108;
  m.thickness = 13;
  m.spacing = 7;

  // Un chiffre seul occupe exactement sa largeur.
  TEST_ASSERT_EQUAL_INT(58, vario::ui::segmentTextWidth("8", m));
  // Deux chiffres : deux largeurs plus une espace.
  TEST_ASSERT_EQUAL_INT(58 * 2 + 7, vario::ui::segmentTextWidth("88", m));
  // Les séparateurs et signes sont plus étroits.
  TEST_ASSERT_TRUE(vario::ui::segmentTextWidth(".", m) < 58);
  TEST_ASSERT_TRUE(vario::ui::segmentTextWidth("-", m) < 58);
  TEST_ASSERT_EQUAL_INT(0, vario::ui::segmentTextWidth("", m));
}

static void test_segment_digits_are_distinguishable() {
  vario::ui::SegmentMetrics m;
  m.width = 40;
  m.height = 70;
  m.thickness = 8;
  m.spacing = 4;

  // Chaque chiffre doit produire un motif de pixels distinct : c'est ce qui
  // garantit qu'on ne confond pas un 6 et un 8 en vol.
  std::vector<std::vector<std::uint8_t>> renders;
  for (char c = '0'; c <= '9'; ++c) {
    auto buffer = makeBuffer();
    Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);
    const char text[2] = {c, '\0'};
    vario::ui::drawSegmentText(canvas, 0, 0, text, m, true);
    renders.push_back(buffer);
  }

  for (std::size_t i = 0; i < renders.size(); ++i) {
    for (std::size_t j = i + 1; j < renders.size(); ++j) {
      TEST_ASSERT_TRUE(renders[i] != renders[j]);
    }
  }
}

static void test_segment_ignores_unknown_characters() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);
  vario::ui::SegmentMetrics m;

  // Un caractère non géré ne doit rien dessiner, mais ne doit pas planter.
  const int end = vario::ui::drawSegmentText(canvas, 0, 0, "Z", m, true);
  TEST_ASSERT_TRUE(end > 0);
  TEST_ASSERT_EQUAL_INT(0, canvas.dirtyLineCount());

  vario::ui::drawSegmentText(canvas, 0, 0, nullptr, m, true);
}

// --- Comparaison aux images de référence ------------------------------------

static void test_vario_screen_matches_golden() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);
  vario::ui::drawVarioScreen(canvas, reference::varioState());
  expectMatchesGolden(canvas, "vario");
}

static void test_calibration_screen_matches_golden() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);
  vario::ui::drawCalibrationScreen(canvas, reference::calibrationProgress());
  expectMatchesGolden(canvas, "calibration");
}

static void test_stats_screen_matches_golden() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  vario::ui::drawStatsScreen(canvas, reference::statsView());
  expectMatchesGolden(canvas, "stats");
}

static void test_menu_screen_matches_golden() {
  auto buffer = makeBuffer();
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  vario::ui::drawMenuScreen(canvas, reference::menuView());
  expectMatchesGolden(canvas, "menu");
}

// Bug 2017 n°8 vu au niveau du pixel : 1,96 m/s doit s'afficher « +2.0 » et non
// « 1.10 ». Le rendu de la valeur fautive et celui de la valeur correcte
// doivent être identiques pixel pour pixel.
static void test_rounding_carry_renders_as_two_point_zero() {
  // On ne compare QUE la zone des grands chiffres. Le reste de l'écran dépend
  // légitimement de la valeur brute : la bande analogique n'a pas la même
  // longueur à 1,96 et à 2,00 m/s.
  constexpr int kDigitsX = 0;
  constexpr int kDigitsY = 20;
  constexpr int kDigitsW = 200;
  constexpr int kDigitsH = 110;

  const auto digitsOf = [](float vario_ms) {
    auto buffer = makeBuffer();
    Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);
    vario::ui::DisplayState s = reference::varioState();
    s.vario_ms = vario_ms;
    vario::ui::drawVarioScreen(canvas, s);

    std::vector<bool> region;
    region.reserve(static_cast<std::size_t>(kDigitsW) * kDigitsH);
    for (int y = kDigitsY; y < kDigitsY + kDigitsH; ++y) {
      for (int x = kDigitsX; x < kDigitsX + kDigitsW; ++x) {
        region.push_back(canvas.pixel(x, y));
      }
    }
    return region;
  };

  // 1,96 m/s doit produire exactement les mêmes pixels que 2,0 m/s.
  // Le code de 2017 affichait « 1.10 » ici.
  TEST_ASSERT_TRUE(digitsOf(1.96f) == digitsOf(2.0f));

  // Et surtout, pas les mêmes que « 1.1 ».
  TEST_ASSERT_TRUE(digitsOf(1.96f) != digitsOf(1.1f));

  // Autres reports : 0,96 → 1.0 et 2,99 → 3.0.
  TEST_ASSERT_TRUE(digitsOf(0.96f) == digitsOf(1.0f));
  TEST_ASSERT_TRUE(digitsOf(2.99f) == digitsOf(3.0f));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_canvas_starts_blank);
  RUN_TEST(test_set_and_read_pixel);
  RUN_TEST(test_out_of_bounds_is_ignored);
  RUN_TEST(test_fill_and_draw_rect);
  RUN_TEST(test_draw_line_endpoints_and_diagonal);
  RUN_TEST(test_text_width_and_rendering);
  RUN_TEST(test_clear_marks_every_line_dirty);
  RUN_TEST(test_only_touched_lines_are_dirty);
  RUN_TEST(test_redundant_write_does_not_dirty);
  RUN_TEST(test_partial_update_touches_few_lines);
  RUN_TEST(test_segment_width_is_consistent);
  RUN_TEST(test_segment_digits_are_distinguishable);
  RUN_TEST(test_segment_ignores_unknown_characters);
  RUN_TEST(test_vario_screen_matches_golden);
  RUN_TEST(test_calibration_screen_matches_golden);
  RUN_TEST(test_stats_screen_matches_golden);
  RUN_TEST(test_menu_screen_matches_golden);
  RUN_TEST(test_rounding_carry_renders_as_two_point_zero);
  return UNITY_END();
}
