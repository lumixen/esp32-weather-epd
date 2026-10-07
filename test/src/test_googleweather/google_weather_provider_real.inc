/* Real Google Weather API response captures for Stockholm, Sweden.
 * Captured 2026-10-05 from currentConditions:lookup, forecast/hours:lookup
 * (24 records), and forecast/days:lookup (5 records). The API key is not
 * included; captures are frozen as offline parser regression fixtures.
 */

static const char kGoogleWeatherCurrentReal[] PROGMEM = R"GOOGLE_CURRENT({
  "timeZone": {
    "id": "Europe/Stockholm",
    "version": ""
  },
  "weatherCondition": {
    "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
    "description": {
      "text": "Cloudy",
      "languageCode": "en"
    },
    "type": "CLOUDY"
  },
  "temperature": {
    "unit": "CELSIUS",
    "degrees": 15.8
  },
  "feelsLikeTemperature": {
    "unit": "CELSIUS",
    "degrees": 14
  },
  "dewPoint": {
    "unit": "CELSIUS",
    "degrees": 12.8
  },
  "heatIndex": {
    "unit": "CELSIUS",
    "degrees": 15.8
  },
  "windChill": {
    "unit": "CELSIUS",
    "degrees": 14
  },
  "precipitation": {
    "probability": {
      "type": "RAIN",
      "percent": 10
    },
    "snowQpf": {
      "unit": "MILLIMETERS",
      "quantity": 0
    },
    "qpf": {
      "unit": "MILLIMETERS",
      "quantity": 0
    }
  },
  "airPressure": {
    "meanSeaLevelMillibars": 1000.22
  },
  "wind": {
    "direction": {
      "cardinal": "WEST_SOUTHWEST",
      "degrees": 242
    },
    "speed": {
      "unit": "KILOMETERS_PER_HOUR",
      "value": 23
    },
    "gust": {
      "unit": "KILOMETERS_PER_HOUR",
      "value": 39
    }
  },
  "visibility": {
    "unit": "KILOMETERS",
    "distance": 16
  },
  "currentConditionsHistory": {
    "temperatureChange": {
      "unit": "CELSIUS",
      "degrees": 4.4
    },
    "maxTemperature": {
      "unit": "CELSIUS",
      "degrees": 16.4
    },
    "minTemperature": {
      "unit": "CELSIUS",
      "degrees": 11.2
    },
    "snowQpf": {
      "unit": "MILLIMETERS",
      "quantity": 0
    },
    "qpf": {
      "unit": "MILLIMETERS",
      "quantity": 2.07
    }
  },
  "currentTime": "2026-10-05T19:11:32.459857334Z",
  "isDaytime": false,
  "relativeHumidity": 82,
  "uvIndex": 0,
  "thunderstormProbability": 0,
  "cloudCover": 99
}
)GOOGLE_CURRENT";

static const char kGoogleWeatherHourlyReal[] PROGMEM = R"GOOGLE_HOURLY({
  "forecastHours": [
    {
      "interval": {
        "startTime": "2026-10-05T19:00:00Z",
        "endTime": "2026-10-05T20:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 5,
        "hours": 21,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
        "description": {
          "text": "Cloudy",
          "languageCode": "en"
        },
        "type": "CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.8
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 12.8
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.8
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 13.9
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 10
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1000.35
      },
      "wind": {
        "direction": {
          "cardinal": "WEST_SOUTHWEST",
          "degrees": 242
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 23
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 39
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 82,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 99
    },
    {
      "interval": {
        "startTime": "2026-10-05T20:00:00Z",
        "endTime": "2026-10-05T21:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 5,
        "hours": 22,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
        "description": {
          "text": "Cloudy",
          "languageCode": "en"
        },
        "type": "CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.7
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 12.6
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.7
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 13.8
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 10
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 999.69
      },
      "wind": {
        "direction": {
          "cardinal": "WEST_SOUTHWEST",
          "degrees": 243
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 24
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 40
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 81,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 98
    },
    {
      "interval": {
        "startTime": "2026-10-05T21:00:00Z",
        "endTime": "2026-10-05T22:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 5,
        "hours": 23,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
        "description": {
          "text": "Cloudy",
          "languageCode": "en"
        },
        "type": "CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.5
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 12.3
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.5
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 13.5
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 10
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 999.41
      },
      "wind": {
        "direction": {
          "cardinal": "WEST_SOUTHWEST",
          "degrees": 248
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 26
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 43
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 81,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 93
    },
    {
      "interval": {
        "startTime": "2026-10-05T22:00:00Z",
        "endTime": "2026-10-05T23:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 0,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
        "description": {
          "text": "Cloudy",
          "languageCode": "en"
        },
        "type": "CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.2
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 11.9
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.2
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 13.2
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 10
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 999.2
      },
      "wind": {
        "direction": {
          "cardinal": "WEST_SOUTHWEST",
          "degrees": 251
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 26
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 42
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 80,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 93
    },
    {
      "interval": {
        "startTime": "2026-10-05T23:00:00Z",
        "endTime": "2026-10-06T00:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 1,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_clear",
        "description": {
          "text": "Partly cloudy",
          "languageCode": "en"
        },
        "type": "PARTLY_CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 12.8
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 5
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 999.13
      },
      "wind": {
        "direction": {
          "cardinal": "WEST_SOUTHWEST",
          "degrees": 258
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 26
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 43
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 78,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 48
    },
    {
      "interval": {
        "startTime": "2026-10-06T00:00:00Z",
        "endTime": "2026-10-06T01:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 2,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_clear",
        "description": {
          "text": "Partly cloudy",
          "languageCode": "en"
        },
        "type": "PARTLY_CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 14.7
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 10.7
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 14.7
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 12.3
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 999.77
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 261
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 26
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 45
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 76,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 39
    },
    {
      "interval": {
        "startTime": "2026-10-06T01:00:00Z",
        "endTime": "2026-10-06T02:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 3,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/mostly_cloudy_night",
        "description": {
          "text": "Mostly cloudy",
          "languageCode": "en"
        },
        "type": "MOSTLY_CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 12
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 9.5
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 12
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 10
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 999.82
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 264
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 24
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 42
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 74,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 66
    },
    {
      "interval": {
        "startTime": "2026-10-06T02:00:00Z",
        "endTime": "2026-10-06T03:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 4,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_clear",
        "description": {
          "text": "Partly cloudy",
          "languageCode": "en"
        },
        "type": "PARTLY_CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 13.3
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 8.4
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 13.3
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 10.5
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 5
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1000.24
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 23
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 39
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 71,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 43
    },
    {
      "interval": {
        "startTime": "2026-10-06T03:00:00Z",
        "endTime": "2026-10-06T04:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 5,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/clear",
        "description": {
          "text": "Clear",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 12.6
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 7.3
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 12.6
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.7
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1000.69
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 267
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 23
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 70,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 3
    },
    {
      "interval": {
        "startTime": "2026-10-06T04:00:00Z",
        "endTime": "2026-10-06T05:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 6,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_clear",
        "description": {
          "text": "Partly cloudy",
          "languageCode": "en"
        },
        "type": "PARTLY_CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 11.9
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 6.4
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 11.9
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1001.36
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 21
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 35
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 69,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 31
    },
    {
      "interval": {
        "startTime": "2026-10-06T05:00:00Z",
        "endTime": "2026-10-06T06:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 7,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/mostly_clear",
        "description": {
          "text": "Clear with periodic clouds",
          "languageCode": "en"
        },
        "type": "MOSTLY_CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 9
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 5.8
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 9
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.4
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1002.11
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 21
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 35
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 68,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 21
    },
    {
      "interval": {
        "startTime": "2026-10-06T06:00:00Z",
        "endTime": "2026-10-06T07:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 8,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_cloudy",
        "description": {
          "text": "Partly sunny",
          "languageCode": "en"
        },
        "type": "PARTLY_CLOUDY"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 9
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 5.6
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 9
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.3
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1002.92
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 265
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 19
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 32
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 67,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 35
    },
    {
      "interval": {
        "startTime": "2026-10-06T07:00:00Z",
        "endTime": "2026-10-06T08:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 9,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/mostly_sunny",
        "description": {
          "text": "Mostly sunny",
          "languageCode": "en"
        },
        "type": "MOSTLY_CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 11.9
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 5.6
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 11.9
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.5
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1003.46
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 265
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 19
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 35
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 65,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 22
    },
    {
      "interval": {
        "startTime": "2026-10-06T08:00:00Z",
        "endTime": "2026-10-06T09:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 10,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 12.8
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 5.5
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 12.8
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.9
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1004.02
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 21
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 60,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 5
    },
    {
      "interval": {
        "startTime": "2026-10-06T09:00:00Z",
        "endTime": "2026-10-06T10:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 11,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 13.8
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 12
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 5.2
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 13.8
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 12
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.3
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1004.36
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 267
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 23
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 56,
      "uvIndex": 1,
      "thunderstormProbability": 0,
      "cloudCover": 15
    },
    {
      "interval": {
        "startTime": "2026-10-06T10:00:00Z",
        "endTime": "2026-10-06T11:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 12,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 14.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.7
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 14.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.4
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1004.64
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 269
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 23
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 52,
      "uvIndex": 1,
      "thunderstormProbability": 0,
      "cloudCover": 8
    },
    {
      "interval": {
        "startTime": "2026-10-06T11:00:00Z",
        "endTime": "2026-10-06T12:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 13,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 14.9
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.3
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 14.9
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.4
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1004.86
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 270
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 23
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 39
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 49,
      "uvIndex": 1,
      "thunderstormProbability": 0,
      "cloudCover": 9
    },
    {
      "interval": {
        "startTime": "2026-10-06T12:00:00Z",
        "endTime": "2026-10-06T13:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 14,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.1
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.6
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1005.43
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 270
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 21
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 46,
      "uvIndex": 1,
      "thunderstormProbability": 0,
      "cloudCover": 1
    },
    {
      "interval": {
        "startTime": "2026-10-06T13:00:00Z",
        "endTime": "2026-10-06T14:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 15,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.5
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.5
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.5
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.7
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1005.76
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 268
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 21
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 47,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 0
    },
    {
      "interval": {
        "startTime": "2026-10-06T14:00:00Z",
        "endTime": "2026-10-06T15:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 16,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 15.2
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.5
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.2
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.6
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1005.98
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 19
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 37
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 49,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 0
    },
    {
      "interval": {
        "startTime": "2026-10-06T15:00:00Z",
        "endTime": "2026-10-06T16:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 17,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 14.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.6
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 14.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 13
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 9.3
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1006.31
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 19
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 32
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 51,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 0
    },
    {
      "interval": {
        "startTime": "2026-10-06T16:00:00Z",
        "endTime": "2026-10-06T17:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 18,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
        "description": {
          "text": "Sunny",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 13.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 12
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.7
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 13.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 12
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.9
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1006.9
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 266
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 18
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 29
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": true,
      "relativeHumidity": 55,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 0
    },
    {
      "interval": {
        "startTime": "2026-10-06T17:00:00Z",
        "endTime": "2026-10-06T18:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 19,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/clear",
        "description": {
          "text": "Clear",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 12.3
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 4.9
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 12.3
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.5
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1007.58
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 263
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 16
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 26
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 60,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 0
    },
    {
      "interval": {
        "startTime": "2026-10-06T18:00:00Z",
        "endTime": "2026-10-06T19:00:00Z"
      },
      "displayDateTime": {
        "year": 2026,
        "month": 10,
        "day": 6,
        "hours": 20,
        "minutes": 0,
        "seconds": 0,
        "nanos": 0,
        "utcOffset": "7200s"
      },
      "weatherCondition": {
        "iconBaseUri": "https://maps.gstatic.com/weather/v1/clear",
        "description": {
          "text": "Clear",
          "languageCode": "en"
        },
        "type": "CLEAR"
      },
      "temperature": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "feelsLikeTemperature": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "dewPoint": {
        "unit": "CELSIUS",
        "degrees": 5.1
      },
      "heatIndex": {
        "unit": "CELSIUS",
        "degrees": 11.4
      },
      "windChill": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "wetBulbTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.1
      },
      "precipitation": {
        "probability": {
          "type": "RAIN",
          "percent": 0
        },
        "snowQpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        },
        "qpf": {
          "unit": "MILLIMETERS",
          "quantity": 0
        }
      },
      "airPressure": {
        "meanSeaLevelMillibars": 1008.37
      },
      "wind": {
        "direction": {
          "cardinal": "WEST",
          "degrees": 260
        },
        "speed": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 14
        },
        "gust": {
          "unit": "KILOMETERS_PER_HOUR",
          "value": 26
        }
      },
      "visibility": {
        "unit": "KILOMETERS",
        "distance": 16
      },
      "iceThickness": {
        "unit": "MILLIMETERS",
        "thickness": 0
      },
      "isDaytime": false,
      "relativeHumidity": 65,
      "uvIndex": 0,
      "thunderstormProbability": 0,
      "cloudCover": 0
    }
  ],
  "timeZone": {
    "id": "Europe/Stockholm",
    "version": ""
  },
  "nextPageToken": ""
}
)GOOGLE_HOURLY";

static const char kGoogleWeatherDailyReal[] PROGMEM = R"GOOGLE_DAILY({
  "forecastDays": [
    {
      "interval": {
        "startTime": "2026-10-05T05:00:00Z",
        "endTime": "2026-10-06T05:00:00Z"
      },
      "displayDate": {
        "year": 2026,
        "month": 10,
        "day": 5
      },
      "daytimeForecast": {
        "interval": {
          "startTime": "2026-10-05T05:00:00Z",
          "endTime": "2026-10-05T17:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
          "description": {
            "text": "Cloudy",
            "languageCode": "en"
          },
          "type": "CLOUDY"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 15
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0.16
          }
        },
        "wind": {
          "direction": {
            "cardinal": "SOUTHWEST",
            "degrees": 230
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 19
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 34
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 86,
        "uvIndex": 0,
        "thunderstormProbability": 10,
        "cloudCover": 100
      },
      "nighttimeForecast": {
        "interval": {
          "startTime": "2026-10-05T17:00:00Z",
          "endTime": "2026-10-06T05:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_clear",
          "description": {
            "text": "Partly cloudy",
            "languageCode": "en"
          },
          "type": "PARTLY_CLOUDY"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 10
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          }
        },
        "wind": {
          "direction": {
            "cardinal": "WEST_SOUTHWEST",
            "degrees": 254
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 26
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 45
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 77,
        "uvIndex": 0,
        "thunderstormProbability": 0,
        "cloudCover": 99
      },
      "maxTemperature": {
        "unit": "CELSIUS",
        "degrees": 16.4
      },
      "minTemperature": {
        "unit": "CELSIUS",
        "degrees": 11.6
      },
      "feelsLikeMaxTemperature": {
        "unit": "CELSIUS",
        "degrees": 16
      },
      "feelsLikeMinTemperature": {
        "unit": "CELSIUS",
        "degrees": 10
      },
      "sunEvents": {
        "sunriseTime": "2026-10-05T05:01:44.660081108Z",
        "sunsetTime": "2026-10-05T16:09:21.496647848Z"
      },
      "moonEvents": {
        "moonPhase": "WANING_CRESCENT",
        "moonriseTimes": [],
        "moonsetTimes": [
          "2026-10-05T15:07:17.101354123Z"
        ]
      },
      "maxHeatIndex": {
        "unit": "CELSIUS",
        "degrees": 16.4
      }
    },
    {
      "interval": {
        "startTime": "2026-10-06T05:00:00Z",
        "endTime": "2026-10-07T05:00:00Z"
      },
      "displayDate": {
        "year": 2026,
        "month": 10,
        "day": 6
      },
      "daytimeForecast": {
        "interval": {
          "startTime": "2026-10-06T05:00:00Z",
          "endTime": "2026-10-06T17:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/sunny",
          "description": {
            "text": "Sunny",
            "languageCode": "en"
          },
          "type": "CLEAR"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 0
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          }
        },
        "wind": {
          "direction": {
            "cardinal": "WEST",
            "degrees": 267
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 23
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 39
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 55,
        "uvIndex": 1,
        "thunderstormProbability": 0,
        "cloudCover": 35
      },
      "nighttimeForecast": {
        "interval": {
          "startTime": "2026-10-06T17:00:00Z",
          "endTime": "2026-10-07T05:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/clear",
          "description": {
            "text": "Clear",
            "languageCode": "en"
          },
          "type": "CLEAR"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 0
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          }
        },
        "wind": {
          "direction": {
            "cardinal": "WEST",
            "degrees": 264
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 16
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 26
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 76,
        "uvIndex": 0,
        "thunderstormProbability": 0,
        "cloudCover": 0
      },
      "maxTemperature": {
        "unit": "CELSIUS",
        "degrees": 15.5
      },
      "minTemperature": {
        "unit": "CELSIUS",
        "degrees": 6.1
      },
      "feelsLikeMaxTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "feelsLikeMinTemperature": {
        "unit": "CELSIUS",
        "degrees": 5
      },
      "sunEvents": {
        "sunriseTime": "2026-10-06T05:04:04.290841637Z",
        "sunsetTime": "2026-10-06T16:06:26.511677391Z"
      },
      "moonEvents": {
        "moonPhase": "WANING_CRESCENT",
        "moonriseTimes": [
          "2026-10-05T22:43:15.810781895Z"
        ],
        "moonsetTimes": [
          "2026-10-06T15:11:11.342433211Z"
        ]
      },
      "maxHeatIndex": {
        "unit": "CELSIUS",
        "degrees": 15.5
      }
    },
    {
      "interval": {
        "startTime": "2026-10-07T05:00:00Z",
        "endTime": "2026-10-08T05:00:00Z"
      },
      "displayDate": {
        "year": 2026,
        "month": 10,
        "day": 7
      },
      "daytimeForecast": {
        "interval": {
          "startTime": "2026-10-07T05:00:00Z",
          "endTime": "2026-10-07T17:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_cloudy",
          "description": {
            "text": "Partly sunny",
            "languageCode": "en"
          },
          "type": "PARTLY_CLOUDY"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 5
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          }
        },
        "wind": {
          "direction": {
            "cardinal": "WEST_SOUTHWEST",
            "degrees": 244
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 6
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 14
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 67,
        "uvIndex": 1,
        "thunderstormProbability": 0,
        "cloudCover": 64
      },
      "nighttimeForecast": {
        "interval": {
          "startTime": "2026-10-07T17:00:00Z",
          "endTime": "2026-10-08T05:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
          "description": {
            "text": "Cloudy",
            "languageCode": "en"
          },
          "type": "CLOUDY"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 30
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0.61
          }
        },
        "wind": {
          "direction": {
            "cardinal": "SOUTH_SOUTHEAST",
            "degrees": 149
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 10
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 13
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 87,
        "uvIndex": 0,
        "thunderstormProbability": 10,
        "cloudCover": 100
      },
      "maxTemperature": {
        "unit": "CELSIUS",
        "degrees": 14.1
      },
      "minTemperature": {
        "unit": "CELSIUS",
        "degrees": 5.6
      },
      "feelsLikeMaxTemperature": {
        "unit": "CELSIUS",
        "degrees": 14
      },
      "feelsLikeMinTemperature": {
        "unit": "CELSIUS",
        "degrees": 4
      },
      "sunEvents": {
        "sunriseTime": "2026-10-07T05:06:24.259737240Z",
        "sunsetTime": "2026-10-07T16:03:31.964247276Z"
      },
      "moonEvents": {
        "moonPhase": "WANING_CRESCENT",
        "moonriseTimes": [
          "2026-10-07T00:23:51.820328724Z"
        ],
        "moonsetTimes": [
          "2026-10-07T15:13:16.786339910Z"
        ]
      },
      "maxHeatIndex": {
        "unit": "CELSIUS",
        "degrees": 14.1
      }
    },
    {
      "interval": {
        "startTime": "2026-10-08T05:00:00Z",
        "endTime": "2026-10-09T05:00:00Z"
      },
      "displayDate": {
        "year": 2026,
        "month": 10,
        "day": 8
      },
      "daytimeForecast": {
        "interval": {
          "startTime": "2026-10-08T05:00:00Z",
          "endTime": "2026-10-08T17:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/showers",
          "description": {
            "text": "Rain",
            "languageCode": "en"
          },
          "type": "RAIN"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 75
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 6.89
          }
        },
        "wind": {
          "direction": {
            "cardinal": "SOUTHEAST",
            "degrees": 131
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 14
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 24
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 92,
        "uvIndex": 0,
        "thunderstormProbability": 10,
        "cloudCover": 100
      },
      "nighttimeForecast": {
        "interval": {
          "startTime": "2026-10-08T17:00:00Z",
          "endTime": "2026-10-09T05:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/showers",
          "description": {
            "text": "Rain",
            "languageCode": "en"
          },
          "type": "RAIN"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 75
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 28.04
          }
        },
        "wind": {
          "direction": {
            "cardinal": "NORTH_NORTHEAST",
            "degrees": 22
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 21
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 32
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 94,
        "uvIndex": 0,
        "thunderstormProbability": 10,
        "cloudCover": 100
      },
      "maxTemperature": {
        "unit": "CELSIUS",
        "degrees": 12.4
      },
      "minTemperature": {
        "unit": "CELSIUS",
        "degrees": 7.1
      },
      "feelsLikeMaxTemperature": {
        "unit": "CELSIUS",
        "degrees": 11
      },
      "feelsLikeMinTemperature": {
        "unit": "CELSIUS",
        "degrees": 4
      },
      "sunEvents": {
        "sunriseTime": "2026-10-08T05:08:44.578969768Z",
        "sunsetTime": "2026-10-08T16:00:37.883017165Z"
      },
      "moonEvents": {
        "moonPhase": "WANING_CRESCENT",
        "moonriseTimes": [
          "2026-10-08T01:59:21.613294894Z"
        ],
        "moonsetTimes": [
          "2026-10-08T15:14:32.531506313Z"
        ]
      },
      "maxHeatIndex": {
        "unit": "CELSIUS",
        "degrees": 12.4
      }
    },
    {
      "interval": {
        "startTime": "2026-10-09T05:00:00Z",
        "endTime": "2026-10-10T05:00:00Z"
      },
      "displayDate": {
        "year": 2026,
        "month": 10,
        "day": 9
      },
      "daytimeForecast": {
        "interval": {
          "startTime": "2026-10-09T05:00:00Z",
          "endTime": "2026-10-09T17:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/drizzle",
          "description": {
            "text": "Light rain",
            "languageCode": "en"
          },
          "type": "LIGHT_RAIN"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 55
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 4.28
          }
        },
        "wind": {
          "direction": {
            "cardinal": "NORTHWEST",
            "degrees": 324
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 26
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 50
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 80,
        "uvIndex": 0,
        "thunderstormProbability": 10,
        "cloudCover": 100
      },
      "nighttimeForecast": {
        "interval": {
          "startTime": "2026-10-09T17:00:00Z",
          "endTime": "2026-10-10T05:00:00Z"
        },
        "weatherCondition": {
          "iconBaseUri": "https://maps.gstatic.com/weather/v1/cloudy",
          "description": {
            "text": "Cloudy",
            "languageCode": "en"
          },
          "type": "CLOUDY"
        },
        "precipitation": {
          "probability": {
            "type": "RAIN",
            "percent": 10
          },
          "snowQpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          },
          "qpf": {
            "unit": "MILLIMETERS",
            "quantity": 0
          }
        },
        "wind": {
          "direction": {
            "cardinal": "NORTHWEST",
            "degrees": 310
          },
          "speed": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 21
          },
          "gust": {
            "unit": "KILOMETERS_PER_HOUR",
            "value": 48
          }
        },
        "iceThickness": {
          "unit": "MILLIMETERS",
          "thickness": 0
        },
        "relativeHumidity": 86,
        "uvIndex": 0,
        "thunderstormProbability": 0,
        "cloudCover": 99
      },
      "maxTemperature": {
        "unit": "CELSIUS",
        "degrees": 8.4
      },
      "minTemperature": {
        "unit": "CELSIUS",
        "degrees": 4.5
      },
      "feelsLikeMaxTemperature": {
        "unit": "CELSIUS",
        "degrees": 5
      },
      "feelsLikeMinTemperature": {
        "unit": "CELSIUS",
        "degrees": 1
      },
      "sunEvents": {
        "sunriseTime": "2026-10-09T05:11:05.259890006Z",
        "sunsetTime": "2026-10-09T15:57:44.297293712Z"
      },
      "moonEvents": {
        "moonPhase": "WANING_CRESCENT",
        "moonriseTimes": [
          "2026-10-09T03:31:24.579879847Z"
        ],
        "moonsetTimes": [
          "2026-10-09T15:15:35.176246989Z"
        ]
      },
      "maxHeatIndex": {
        "unit": "CELSIUS",
        "degrees": 8.4
      }
    }
  ],
  "timeZone": {
    "id": "Europe/Stockholm",
    "version": ""
  },
  "nextPageToken": ""
}
)GOOGLE_DAILY";
