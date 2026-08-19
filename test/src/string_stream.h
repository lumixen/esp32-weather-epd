/* Shared test helper: a minimal read-only Stream over a String.
 *
 * The framework's StreamString does not expose String assignment in this
 * Arduino core, so suites parse fixtures through this wrapper. The String is
 * copied (not borrowed), which makes construction from temporaries safe
 * (e.g. GapStream(kFeedUkraineReal, 512) builds an implicit String); read()
 * and peek() return the byte as unsigned (0..255, -1 at EOF) so UTF-8
 * payloads (the "µ" units markers, Cyrillic alert text) never promote signed
 * chars to unexpected negative values. bytesRead() counts consumed bytes for
 * tests that assert how much of a document was read.
 *
 * GPL-3.0, see LICENSE.
 */
#pragma once

#include <Arduino.h>

class StringStream : public Stream {
 public:
  explicit StringStream(const String &s) : data_(s), pos_(0), bytesRead_(0) {}
  int read() override {
    if (pos_ < data_.length()) {
      ++bytesRead_;
      return static_cast<uint8_t>(data_[pos_++]);
    }
    return -1;
  }
  int peek() override {
    return pos_ < data_.length() ? static_cast<uint8_t>(data_[pos_]) : -1;
  }
  int available() override { return data_.length() - pos_; }
  size_t write(uint8_t) override { return 0; }
  void flush() override {}
  size_t bytesRead() const { return bytesRead_; }

 private:
  String data_;
  size_t pos_;
  size_t bytesRead_;
};