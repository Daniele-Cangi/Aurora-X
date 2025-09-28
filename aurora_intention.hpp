#pragma once
#include <string>

// ---------- Intentions ----------
struct Intention {
  double deadline_s = 600;
  double reliability = 0.99;
  double duty_frac = 0.01;
  bool   allow_optical = true;
  bool   allow_backscatter = true;
  int    ris_tiles = 16;

  static Intention parse(const std::string& s) {
    // parser che legge i parametri dalla stringa "deadline:600; reliability:0.99; …"
    // Implementazione da aggiungere se serve
    return {};
  }
};
