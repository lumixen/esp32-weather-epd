/* Shared test helper: a minimal read-only Stream over a String.
 *
 * The framework's StreamString does not expose String assignment in this
 * Arduino core, so suites parse fixtures through this wrapper. To keep peak
 * heap low, the String is borrowed, not copied, whenever the caller already
 * owns it (the stream must not outlive it); construction from a C string -
 * char arrays, literals, temporaries built by implicit conversion - owns a
 * private copy instead. The rvalue String constructor is deleted so a
 * temporary can never be silently borrowed, and the copy/move operations are
 * deleted too: duplicating a stream would copy the data_ pointer verbatim
 * and, for a stream owning its String, leave it pointing at the other
 * object's owned_ after that object dies. Streams are stack locals, used in
 * place. read() and peek() return the byte as unsigned (0..255, -1 at EOF)
 * so UTF-8 payloads (the "µ" units markers, Cyrillic alert text) never
 * promote signed chars to unexpected negative values. bytesRead() counts
 * consumed bytes for tests that assert how much of a document was read.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */
#pragma once

#include <Arduino.h>

class StringStream : public Stream {
 public:
  /* Borrows `s`; the caller must keep it alive while the stream exists. */
  explicit StringStream(const String &s) : data_(&s), pos_(0), bytesRead_(0) {}
  /* Copies `s`; safe for char arrays, literals and other ephemeral sources. */
  explicit StringStream(const char *s) : owned_(s), data_(&owned_), pos_(0), bytesRead_(0) {}
  StringStream(String &&) = delete;  // a temporary must not be borrowed
  StringStream(const StringStream &) = delete;
  StringStream &operator=(const StringStream &) = delete;
  StringStream(StringStream &&) = delete;
  StringStream &operator=(StringStream &&) = delete;
  int read() override {
    if (pos_ < data_->length()) {
      ++bytesRead_;
      return static_cast<uint8_t>((*data_)[pos_++]);
    }
    return -1;
  }
  int peek() override { return pos_ < data_->length() ? static_cast<uint8_t>((*data_)[pos_]) : -1; }
  int available() override { return data_->length() - pos_; }
  size_t write(uint8_t) override { return 0; }
  void flush() override {}
  size_t bytesRead() const { return bytesRead_; }

 private:
  String owned_;        // empty when borrowing
  const String *data_;  // &owned_ or the borrowed String
  size_t pos_;
  size_t bytesRead_;
};