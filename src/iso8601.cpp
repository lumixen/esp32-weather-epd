/* ISO 8601 timestamp parsing for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "iso8601.h"

namespace {

bool isDigit(char value) { return value >= '0' && value <= '9'; }

bool readDigits(const char *&cursor, int count, int &value) {
  int result = 0;
  for (int i = 0; i < count; ++i) {
    if (!isDigit(cursor[i]))
      return false;
    result = result * 10 + (cursor[i] - '0');
  }
  cursor += count;
  value = result;
  return true;
}

bool leapYear(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

bool validDate(int year, int month, int day) {
  static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return year >= 0 && month >= 1 && month <= 12 && day >= 1 &&
         day <= days[month] + (month == 2 && leapYear(year) ? 1 : 0);
}

/* Days from the civil epoch (1970-01-01), based on Howard Hinnant's date
 * algorithms. The proleptic Gregorian calendar is used. */
int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
}

}  // namespace

namespace iso8601 {

bool parse(const char *value, int64_t &epoch) {
  epoch = 0;
  if (value == nullptr)
    return false;

  const char *cursor = value;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  if (!readDigits(cursor, 4, year) || *cursor++ != '-' || !readDigits(cursor, 2, month) || *cursor++ != '-' ||
      !readDigits(cursor, 2, day) || (*cursor != 'T' && *cursor != 't'))
    return false;
  ++cursor;
  if (!readDigits(cursor, 2, hour) || *cursor++ != ':' || !readDigits(cursor, 2, minute) || *cursor++ != ':' ||
      !readDigits(cursor, 2, second))
    return false;
  if (!validDate(year, month, day) || hour > 23 || minute > 59 || second > 60)
    return false;

  if (*cursor == '.') {
    ++cursor;
    const char *fractionStart = cursor;
    while (isDigit(*cursor))
      ++cursor;
    if (cursor == fractionStart)
      return false;
  }

  int offsetMinutes = 0;
  if (*cursor == 'Z' || *cursor == 'z') {
    ++cursor;
  } else if (*cursor == '+' || *cursor == '-') {
    const int sign = *cursor++ == '+' ? 1 : -1;
    int offsetHours;
    int offsetMinutePart;
    if (!readDigits(cursor, 2, offsetHours) || *cursor++ != ':' || !readDigits(cursor, 2, offsetMinutePart) ||
        offsetHours > 23 || offsetMinutePart > 59)
      return false;
    offsetMinutes = sign * (offsetHours * 60 + offsetMinutePart);
  } else {
    return false;
  }
  if (*cursor != '\0')
    return false;

  epoch = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400LL + hour * 3600LL +
          minute * 60LL + second - offsetMinutes * 60LL;
  return true;
}

}  // namespace iso8601
