// Génère les images de référence utilisées par test/test_render.
//
// À relancer volontairement (make golden) après une modification assumée de la
// mise en page, JAMAIS pour faire taire un test qui échoue sans avoir regardé
// l'image obtenue dans build/*-obtenu.pbm.

#include <cstdio>
#include <string>
#include <vector>

#include "support/reference_states.h"
#include "ui/canvas.h"
#include "ui/screens.h"

namespace {

using vario::ui::Canvas;
using vario::ui::kScreenHeight;
using vario::ui::kScreenWidth;

bool writePbm(const std::string& path, const Canvas& canvas) {
  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) return false;

  std::fprintf(file, "P4\n%d %d\n", canvas.width(), canvas.height());
  const std::size_t bytes = static_cast<std::size_t>(canvas.stride()) * canvas.height();
  const bool ok = std::fwrite(canvas.buffer(), 1, bytes, file) == bytes;
  std::fclose(file);
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = (argc > 1) ? argv[1] : "test/golden";

  std::vector<std::uint8_t> buffer(
      static_cast<std::size_t>((kScreenWidth + 7) / 8) * kScreenHeight, 0);
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  int written = 0;
  const auto save = [&](const char* name) {
    if (writePbm(out + "/" + name + std::string(".pbm"), canvas)) ++written;
  };

  vario::ui::drawVarioScreen(canvas, reference::varioState());
  save("vario");

  vario::ui::drawCalibrationScreen(canvas, reference::calibrationProgress());
  save("calibration");

  vario::ui::drawStatsScreen(canvas, reference::statsView());
  save("stats");

  vario::ui::drawMenuScreen(canvas, reference::menuView());
  save("menu");

  std::printf("%d images de reference ecrites dans %s\n", written, out.c_str());
  return written == 4 ? 0 : 1;
}
