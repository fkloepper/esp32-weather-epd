/* API response deserialization for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
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

#include <cmath>
#include <vector>
#include <ArduinoJson.h>
#include "api_response.h"
#include "config.h"

// Compute moon phase fraction [0.0, 1.0) from a Unix timestamp.
// 0.0 / 1.0 = new moon, 0.5 = full moon.
// Reference new moon: 2000-01-06 18:14 UTC = 947190840 sec
static float calcMoonPhase(int64_t unix_time)
{
  const double new_moon_epoch = 947190840.0;
  const double lunar_cycle    = 29.53058770576 * 86400.0; // seconds
  double elapsed = (double)unix_time - new_moon_epoch;
  double phase   = fmod(elapsed / lunar_cycle, 1.0);
  if (phase < 0.0) phase += 1.0;
  return (float)phase;
}

// Parse Open-Meteo forecast JSON into weather_forecast_t.
// Open-Meteo returns temperatures in Celsius; stored as Kelvin (+273.15)
// so all existing conversion/display code remains unchanged.
// Wind speed is requested in m/s to match existing conversion functions.
// WMO weather codes are stored in weather.id;
// weather.icon is set to "01d" or "01n" so isDay() keeps working.
DeserializationError deserializeForecast(WiFiClient &json,
                                        weather_forecast_t &r)
{
  // Open-Meteo JSON layout:
  // {
  //   "latitude": f, "longitude": f,
  //   "utc_offset_seconds": n, "timezone": "...",
  //   "current": { "time": unix, "temperature_2m": f, "is_day": 0|1, ... },
  //   "hourly":  { "time": [...], "temperature_2m": [...], ... },
  //   "daily":   { "time": [...], "temperature_2m_max": [...], ... }
  // }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) { return error; }

  r.lat             = doc["latitude"]           .as<float>();
  r.lon             = doc["longitude"]          .as<float>();
  r.timezone        = doc["timezone"]           .as<const char *>();
  r.timezone_offset = doc["utc_offset_seconds"] .as<int>();

  // ---- current ----
  JsonObject cur = doc["current"];
  r.current.dt         = cur["time"]                    .as<int64_t>();
  r.current.temp       = cur["temperature_2m"]          .as<float>() + 273.15f;
  r.current.feels_like = cur["apparent_temperature"]    .as<float>() + 273.15f;
  r.current.pressure   = cur["pressure_msl"]            .as<int>();
  r.current.humidity   = cur["relative_humidity_2m"]    .as<int>();
  r.current.dew_point  = cur["dew_point_2m"]            .as<float>() + 273.15f;
  r.current.clouds     = cur["cloud_cover"]             .as<int>();
  r.current.uvi        = cur["uv_index"]                .as<float>();
  r.current.visibility = cur["visibility"]              .as<int>();
  r.current.wind_speed = cur["wind_speed_10m"]          .as<float>();
  r.current.wind_gust  = cur["wind_gusts_10m"]          .as<float>();
  r.current.wind_deg   = cur["wind_direction_10m"]      .as<int>();
  r.current.rain_1h    = cur["precipitation"]           .as<float>();
  r.current.snow_1h    = 0.f;
  r.current.weather.id          = cur["weather_code"]  .as<int>();
  r.current.weather.icon        = cur["is_day"].as<int>() ? "01d" : "01n";
  r.current.weather.main        = "";
  r.current.weather.description = "";

  // Sunrise/sunset come from daily[0]
  JsonArray d_sunrise = doc["daily"]["sunrise"].as<JsonArray>();
  JsonArray d_sunset  = doc["daily"]["sunset"] .as<JsonArray>();
  r.current.sunrise = d_sunrise[0].as<int64_t>();
  r.current.sunset  = d_sunset[0] .as<int64_t>();

  // ---- hourly ----
  JsonArray h_time   = doc["hourly"]["time"];
  JsonArray h_temp   = doc["hourly"]["temperature_2m"];
  JsonArray h_feels  = doc["hourly"]["apparent_temperature"];
  JsonArray h_pres   = doc["hourly"]["pressure_msl"];
  JsonArray h_hum    = doc["hourly"]["relative_humidity_2m"];
  JsonArray h_dew    = doc["hourly"]["dew_point_2m"];
  JsonArray h_cloud  = doc["hourly"]["cloud_cover"];
  JsonArray h_uvi    = doc["hourly"]["uv_index"];
  JsonArray h_vis    = doc["hourly"]["visibility"];
  JsonArray h_wspd   = doc["hourly"]["wind_speed_10m"];
  JsonArray h_wgust  = doc["hourly"]["wind_gusts_10m"];
  JsonArray h_wdir   = doc["hourly"]["wind_direction_10m"];
  JsonArray h_pop    = doc["hourly"]["precipitation_probability"];
  JsonArray h_precip = doc["hourly"]["precipitation"];
  JsonArray h_wcode  = doc["hourly"]["weather_code"];
  JsonArray h_isday  = doc["hourly"]["is_day"];

  int hCount = (int)h_time.size();
  for (int i = 0; i < NUM_HOURLY && i < hCount; ++i)
  {
    r.hourly[i].dt         = h_time[i]  .as<int64_t>();
    r.hourly[i].temp       = h_temp[i]  .as<float>() + 273.15f;
    r.hourly[i].feels_like = h_feels[i] .as<float>() + 273.15f;
    r.hourly[i].pressure   = h_pres[i]  .as<int>();
    r.hourly[i].humidity   = h_hum[i]   .as<int>();
    r.hourly[i].dew_point  = h_dew[i]   .as<float>() + 273.15f;
    r.hourly[i].clouds     = h_cloud[i] .as<int>();
    r.hourly[i].uvi        = h_uvi[i]   .as<float>();
    r.hourly[i].visibility = h_vis[i]   .as<int>();
    r.hourly[i].wind_speed = h_wspd[i]  .as<float>();
    r.hourly[i].wind_gust  = h_wgust[i] .as<float>();
    r.hourly[i].wind_deg   = h_wdir[i]  .as<int>();
    // pop from Open-Meteo is in % [0..100]; OWM uses [0..1]
    r.hourly[i].pop        = h_pop[i]   .as<float>() / 100.0f;
    r.hourly[i].rain_1h    = h_precip[i].as<float>(); // mm WE (rain+shower+snow)
    r.hourly[i].snow_1h    = 0.f;
    r.hourly[i].weather.id   = h_wcode[i].as<int>();
    r.hourly[i].weather.icon = h_isday[i].as<int>() ? "01d" : "01n";
    r.hourly[i].weather.main = "";
    r.hourly[i].weather.description = "";
  }

  // ---- daily ----
  JsonArray d_time  = doc["daily"]["time"];
  JsonArray d_tmax  = doc["daily"]["temperature_2m_max"];
  JsonArray d_tmin  = doc["daily"]["temperature_2m_min"];
  JsonArray d_uvi   = doc["daily"]["uv_index_max"];
  JsonArray d_precip = doc["daily"]["precipitation_sum"];
  JsonArray d_pop   = doc["daily"]["precipitation_probability_max"];
  JsonArray d_wcode = doc["daily"]["weather_code"];
  JsonArray d_wspd  = doc["daily"]["wind_speed_10m_max"];
  JsonArray d_wgust = doc["daily"]["wind_gusts_10m_max"];
  JsonArray d_wdir  = doc["daily"]["wind_direction_10m_dominant"];

  int dCount = (int)d_time.size();
  for (int i = 0; i < NUM_DAILY && i < dCount; ++i)
  {
    r.daily[i].dt        = d_time[i].as<int64_t>();
    r.daily[i].sunrise   = d_sunrise[i].as<int64_t>();
    r.daily[i].sunset    = d_sunset[i] .as<int64_t>();
    // Moon phase computed from date; moonrise/moonset span the whole day so
    // isMoonInSky() returns true at night whenever moon_phase != 0/1.
    r.daily[i].moon_phase = calcMoonPhase(r.daily[i].dt);
    r.daily[i].moonrise   = r.daily[i].dt;
    r.daily[i].moonset    = r.daily[i].dt + 86400;
    float tmax = d_tmax[i].as<float>() + 273.15f;
    float tmin = d_tmin[i].as<float>() + 273.15f;
    r.daily[i].temp.max   = tmax;
    r.daily[i].temp.min   = tmin;
    r.daily[i].temp.day   = tmax;
    r.daily[i].temp.morn  = tmin;
    r.daily[i].temp.eve   = tmax;
    r.daily[i].temp.night = tmin;
    r.daily[i].uvi        = d_uvi[i]   .as<float>();
    // pop from Open-Meteo daily is in % [0..100]
    r.daily[i].pop        = d_pop[i]   .as<float>() / 100.0f;
    // precipitation_sum includes rain + snow (mm water equivalent)
    r.daily[i].rain       = d_precip[i].as<float>();
    r.daily[i].snow       = 0.f;
    r.daily[i].wind_speed = d_wspd[i]  .as<float>();
    r.daily[i].wind_gust  = d_wgust[i] .as<float>();
    r.daily[i].wind_deg   = d_wdir[i]  .as<int>();
    r.daily[i].weather.id   = d_wcode[i].as<int>();
    r.daily[i].weather.icon = "01d";
    r.daily[i].weather.main = "";
    r.daily[i].weather.description = "";
  }

  // Open-Meteo does not provide weather alerts; leave the vector empty.
  r.alerts.clear();

  return error;
} // end deserializeForecast

// Parse Open-Meteo air quality JSON into weather_air_quality_t.
// Returns the past NUM_AIR_POLLUTION hourly readings.
// Note: Open-Meteo does not provide nitrogen monoxide (NO); stored as 0.
DeserializationError deserializeAirQuality(WiFiClient &json,
                                           weather_air_quality_t &r)
{
  int i = 0;

  // Open-Meteo Air Quality JSON layout:
  // {
  //   "latitude": f, "longitude": f,
  //   "hourly": {
  //     "time": [...],
  //     "carbon_monoxide": [...], "nitrogen_dioxide": [...],
  //     "sulphur_dioxide": [...], "ozone": [...],
  //     "pm2_5": [...], "pm10": [...], "ammonia": [...]
  //   }
  // }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) { return error; }

  r.coord.lat = doc["latitude"] .as<float>();
  r.coord.lon = doc["longitude"].as<float>();

  JsonArray h_time = doc["hourly"]["time"];
  JsonArray h_co   = doc["hourly"]["carbon_monoxide"];
  JsonArray h_no2  = doc["hourly"]["nitrogen_dioxide"];
  JsonArray h_so2  = doc["hourly"]["sulphur_dioxide"];
  JsonArray h_o3   = doc["hourly"]["ozone"];
  JsonArray h_pm25 = doc["hourly"]["pm2_5"];
  JsonArray h_pm10 = doc["hourly"]["pm10"];
  JsonArray h_nh3  = doc["hourly"]["ammonia"];

  int count = (int)h_time.size();
  if (count > NUM_AIR_POLLUTION) count = NUM_AIR_POLLUTION;
  for (int i = 0; i < count; ++i)
  {
    r.dt[i]               = h_time[i].as<int64_t>();
    r.main_aqi[i]         = 0; // OWM 1-5 scale not used with Open-Meteo
    r.components.co[i]    = h_co[i]  .as<float>();
    r.components.no[i]    = 0.f;     // not provided by Open-Meteo
    r.components.no2[i]   = h_no2[i] .as<float>();
    r.components.o3[i]    = h_o3[i]  .as<float>();
    r.components.so2[i]   = h_so2[i] .as<float>();
    r.components.pm2_5[i] = h_pm25[i].as<float>();
    r.components.pm10[i]  = h_pm10[i].as<float>();
    r.components.nh3[i]   = h_nh3[i] .as<float>(); // Europe only, 0 elsewhere
  }

  return error;
} // end deserializeAirQuality

