#pragma once

#include <vector>

namespace chalkwalk::physical {

// Empty stub for the FFT-extracted modal-data import pipeline (spec §4.2,
// "custom FFT-extracted objects"). The real pipeline will:
//   1. Take an audio sample of a struck object.
//   2. STFT analysis -> peak picking on the long-term average spectrum.
//   3. Per-peak: estimate frequency, decay rate (T60), spatial amplitude.
//   4. Emit a ModeList that the ModalResonator can configure itself from.
//
// This class exists so the architectural seam is present; no behaviour yet.

struct Mode {
  float frequencyHz = 0.0f;
  float decayRate   = 0.0f;
  float amplitude   = 0.0f;
};

using ModeList = std::vector<Mode>;

class ModalDataImporter {
 public:
  // TODO: importFromFile / importFromAudioBuffer. Both belong in the PLUGIN
  // rather than here -- a file is a host concern, and this library must not
  // learn what one is.
};

}  // namespace chalkwalk::physical
