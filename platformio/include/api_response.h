/* API response deserialization declarations for esp32-weather-epd.
 * Copyright (C) 2022-2023  Luke Marzen
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

#ifndef __API_RESPONSE_H__
#define __API_RESPONSE_H__

#include <cstdint>
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define NUM_MINUTELY       1 // 61
#define NUM_HOURLY        48 // 48
#define NUM_DAILY          8 // 8
#define NUM_ALERTS         8 // The API does not specify a limit, but if you need more alerts you are probably doomed.
#define NUM_AIR_POLLUTION 24 // Depending on AQI scale, hourly concentrations will need to be averaged over a period of 1h to 24h

typedef struct weather_cond
{
  int     id;               // Weather condition id
  String  main;             // Group of weather parameters (Rain, Snow, Extreme etc.)
  String  description;      // Weather condition within the group (full list of weather conditions). Get the output in your language
  String  icon;             // Weather icon id.
} weather_cond_t;

/*
 * Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
 */
typedef struct weather_temp
{
  float   morn;             // Morning temperature.
  float   day;              // Day temperature.
  float   eve;              // Evening temperature.
  float   night;            // Night temperature.
  float   min;              // Min daily temperature.
  float   max;              // Max daily temperature.
} weather_temp_t;

/*
 * This accounts for the human perception of weather. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
 */
typedef struct weather_feels_like
{
  float   morn;             // Morning temperature.
  float   day;              // Day temperature.
  float   eve;              // Evening temperature.
  float   night;            // Night temperature.
} weather_feels_like_t;

/*
 * Current weather data API response
 */
typedef struct weather_current
{
  int64_t dt;               // Current time, Unix, UTC
  int64_t sunrise;          // Sunrise time, Unix, UTC
  int64_t sunset;           // Sunset time, Unix, UTC
  float   temp;             // Temperature. Units - default: kelvin, metric: Celsius, imperial: Fahrenheit.
  float   feels_like;       // Temperature. This temperature parameter accounts for the human perception of weather. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     pressure;         // Atmospheric pressure on the sea level, hPa
  int     humidity;         // Humidity, %
  float   dew_point;        // Atmospheric temperature (varying according to pressure and humidity) below which water droplets begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     clouds;           // Cloudiness, %
  float   uvi;              // Current UV index
  int     visibility;       // Average visibility, metres. The maximum value of the visibility is 10km
  float   wind_speed;       // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float   wind_gust;        // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int     wind_deg;         // Wind direction, degrees (meteorological)
  float   rain_1h;          // (where available) Rain volume for last hour, mm
  float   snow_1h;          // (where available) Snow volume for last hour, mm
  weather_cond_t         weather;
} weather_current_t;

/*
 * Minute forecast weather data API response
 */
typedef struct weather_minutely
{
  int64_t dt;               // Time of the forecasted data, unix, UTC
  float   precipitation;    // Precipitation volume, mm
} weather_minutely_t;

/*
 * Hourly forecast weather data API response
 */
typedef struct weather_hourly
{
  int64_t dt;               // Time of the forecasted data, unix, UTC
  float   temp;             // Temperature. Units - default: kelvin, metric: Celsius, imperial: Fahrenheit.
  float   feels_like;       // Temperature. This temperature parameter accounts for the human perception of weather. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     pressure;         // Atmospheric pressure on the sea level, hPa
  int     humidity;         // Humidity, %
  float   dew_point;        // Atmospheric temperature (varying according to pressure and humidity) below which water droplets begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     clouds;           // Cloudiness, %
  float   uvi;              // Current UV index
  int     visibility;       // Average visibility, metres. The maximum value of the visibility is 10km
  float   wind_speed;       // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float   wind_gust;        // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int     wind_deg;         // Wind direction, degrees (meteorological)
  float   pop;              // Probability of precipitation. The values of the parameter vary between 0 and 1, where 0 is equal to 0%, 1 is equal to 100%
  float   rain_1h;          // (where available) Rain volume for last hour, mm
  float   snow_1h;          // (where available) Snow volume for last hour, mm
  weather_cond_t         weather;
} weather_hourly_t;

/*
 * Daily forecast weather data API response
 */
typedef struct weather_daily
{
  int64_t dt;               // Time of the forecasted data, unix, UTC
  int64_t sunrise;          // Sunrise time, Unix, UTC
  int64_t sunset;           // Sunset time, Unix, UTC
  int64_t moonrise;         // The time of when the moon rises for this day, Unix, UTC
  int64_t moonset;          // The time of when the moon sets for this day, Unix, UTC
  float   moon_phase;       // Moon phase. 0 and 1 are 'new moon', 0.25 is 'first quarter moon', 0.5 is 'full moon' and 0.75 is 'last quarter moon'. The periods in between are called 'waxing crescent', 'waxing gibous', 'waning gibous', and 'waning crescent', respectively.
  weather_temp_t            temp;
  weather_feels_like_t  feels_like;
  int     pressure;         // Atmospheric pressure on the sea level, hPa
  int     humidity;         // Humidity, %
  float   dew_point;        // Atmospheric temperature (varying according to pressure and humidity) below which water droplets begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int     clouds;           // Cloudiness, %
  float   uvi;              // Current UV index
  int     visibility;       // Average visibility, metres. The maximum value of the visibility is 10km
  float   wind_speed;       // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float   wind_gust;        // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int     wind_deg;         // Wind direction, degrees (meteorological)
  float   pop;              // Probability of precipitation. The values of the parameter vary between 0 and 1, where 0 is equal to 0%, 1 is equal to 100%
  float   rain;             // (where available) Precipitation volume, mm
  float   snow;             // (where available) Snow volume, mm
  weather_cond_t         weather;
} weather_daily_t;

/*
 * National weather alerts data from major national weather warning systems
 */
typedef struct weather_alert
{
  String  sender_name;      // Name of the alert source.
  String  event;            // Alert event name
  int64_t start;            // Date and time of the start of the alert, Unix, UTC
  int64_t end;              // Date and time of the end of the alert, Unix, UTC
  String  description;      // Description of the alert
  String  tags;             // Type of severe weather
} weather_alert_t;

/*
 * Response from Open-Meteo Forecast API
 *
 * https://Open-Meteo.org/api/one-call-api
 */
typedef struct weather_forecast
{
  float   lat;              // Geographical coordinates of the location (latitude)
  float   lon;              // Geographical coordinates of the location (longitude)
  String  timezone;         // Timezone name for the requested location
  int     timezone_offset;  // Shift in seconds from UTC
  weather_current_t   current;
  // weather_minutely_t  minutely[NUM_MINUTELY];

  weather_hourly_t    hourly[NUM_HOURLY];
  weather_daily_t     daily[NUM_DAILY];
  std::vector<weather_alert_t> alerts;
} weather_forecast_t;

/*
 * Coordinates from the specified location (latitude, longitude)
 */
typedef struct weather_coord
{
  float   lat;
  float   lon;
} weather_coord_t;

typedef struct weather_air_components
{
  float   co[NUM_AIR_POLLUTION];    // Сoncentration of CO (Carbon monoxide), μg/m^3
  float   no[NUM_AIR_POLLUTION];    // Сoncentration of NO (Nitrogen monoxide), μg/m^3
  float   no2[NUM_AIR_POLLUTION];   // Сoncentration of NO2 (Nitrogen dioxide), μg/m^3
  float   o3[NUM_AIR_POLLUTION];    // Сoncentration of O3 (Ozone), μg/m^3
  float   so2[NUM_AIR_POLLUTION];   // Сoncentration of SO2 (Sulphur dioxide), μg/m^3
  float   pm2_5[NUM_AIR_POLLUTION]; // Сoncentration of PM2.5 (Fine particles matter), μg/m^3
  float   pm10[NUM_AIR_POLLUTION];  // Сoncentration of PM10 (Coarse particulate matter), μg/m^3
  float   nh3[NUM_AIR_POLLUTION];   // Сoncentration of NH3 (Ammonia), μg/m^3
} weather_air_components_t;

/*
 * Response from Open-Meteo Air Quality API
 */
typedef struct weather_air_quality
{
  weather_coord_t      coord;
  int              main_aqi[NUM_AIR_POLLUTION];   // Air Quality Index. Possible values: 1, 2, 3, 4, 5. Where 1 = Good, 2 = Fair, 3 = Moderate, 4 = Poor, 5 = Very Poor.
  weather_air_components_t components;
  int64_t          dt[NUM_AIR_POLLUTION];         // Date and time, Unix, UTC;
} weather_air_quality_t;

DeserializationError deserializeForecast(WiFiClient &json,
                                        weather_forecast_t &r);
DeserializationError deserializeAirQuality(WiFiClient &json,
                                           weather_air_quality_t &r);

/*
 * A single waste pickup entry from the myMüll API
 */
typedef struct waste_pickup
{
  String title; // e.g. "Biotonne 120l-240l"
  String day;   // "YYYY-MM-DD"
  String color; // hex color without '#', e.g. "b8881f"
} waste_pickup_t;

/*
 * Result of getWasteCollection() – next upcoming pickup + days until
 */
typedef struct waste_collection
{
  waste_pickup_t next;  // the next upcoming pickup
  int            days;  // 0 = today, 1 = tomorrow, ...
  bool           valid; // false if no data available
} waste_collection_t;


#endif

