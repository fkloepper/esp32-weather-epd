/* Client side utilities for esp32-weather-epd.
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

// built-in C++ libraries
#include <cstring>
#include <vector>

// arduino/esp32 libraries
#include <Arduino.h>
#include <esp_sntp.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>
#include <Preferences.h>

// additional libraries
#include <Adafruit_BusIO_Register.h>
#include <ArduinoJson.h>

// header files
#include "_locale.h"
#include "api_response.h"
#include "aqi.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "renderer.h"
#ifndef USE_HTTP
  #include <WiFiClientSecure.h>
#endif

#ifdef USE_HTTP
  static const uint16_t OPENMETEO_PORT = 80;
#else
  static const uint16_t OPENMETEO_PORT = 443;
#endif

/* Power-on and connect WiFi.
 * Takes int parameter to store WiFi RSSI, or “Received Signal Strength
 * Indicator"
 *
 * Returns WiFi status.
 */
wl_status_t startWiFi(int &wifiRSSI)
{
  WiFi.mode(WIFI_STA);
  Serial.printf("%s '%s'", TXT_CONNECTING_TO, WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // timeout if WiFi does not connect in WIFI_TIMEOUT ms from now
  unsigned long timeout = millis() + WIFI_TIMEOUT;
  wl_status_t connection_status = WiFi.status();

  while ((connection_status != WL_CONNECTED) && (millis() < timeout))
  {
    Serial.print(".");
    delay(50);
    connection_status = WiFi.status();
  }
  Serial.println();

  if (connection_status == WL_CONNECTED)
  {
    wifiRSSI = WiFi.RSSI(); // get WiFi signal strength now, because the WiFi
                            // will be turned off to save power!
    Serial.println("IP: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.printf("%s '%s'\n", TXT_COULD_NOT_CONNECT_TO, WIFI_SSID);
  }
  return connection_status;
} // startWiFi

/* Disconnect and power-off WiFi.
 */
void killWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
} // killWiFi

/* Prints the local time to serial monitor.
 *
 * Returns true if getting local time was a success, otherwise false.
 */
bool printLocalTime(tm *timeInfo)
{
  int attempts = 0;
  while (!getLocalTime(timeInfo) && attempts++ < 3)
  {
    Serial.println(TXT_FAILED_TO_GET_TIME);
    return false;
  }
  Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S");
  return true;
} // printLocalTime

/* Waits for NTP server time sync, adjusted for the time zone specified in
 * config.cpp.
 *
 * Returns true if time was set successfully, otherwise false.
 *
 * Note: Must be connected to WiFi to get time from NTP server.
 */
bool waitForSNTPSync(tm *timeInfo)
{
  // Wait for SNTP synchronization to complete
  unsigned long timeout = millis() + NTP_TIMEOUT;
  if ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
      && (millis() < timeout))
  {
    Serial.print(TXT_WAITING_FOR_SNTP);
    delay(100); // ms
    while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
        && (millis() < timeout))
    {
      Serial.print(".");
      delay(100); // ms
    }
    Serial.println();
  }
  return printLocalTime(timeInfo);
} // waitForSNTPSync

/* Perform an HTTPS GET request to Open-Meteo's Forecast API.
 * Data is parsed and stored in r.
 *
 * Returns the HTTP Status Code.
 */
#ifdef USE_HTTP
  int getOpenMeteoForecast(WiFiClient &client, weather_forecast_t &r)
#else
  int getOpenMeteoForecast(WiFiClientSecure &client, weather_forecast_t &r)
#endif
{
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};

  String uri = "/v1/forecast?latitude=" + LAT + "&longitude=" + LON
    + "&timeformat=unixtime"
    + "&wind_speed_unit=ms"
    + "&forecast_days=8"
    + "&forecast_hours=" + String(NUM_HOURLY)
    + "&timezone=auto"
    + "&current=temperature_2m,relative_humidity_2m,apparent_temperature"
      ",is_day,precipitation,weather_code,cloud_cover,pressure_msl"
      ",wind_speed_10m,wind_direction_10m,wind_gusts_10m"
      ",uv_index,visibility,dew_point_2m"
    + "&hourly=temperature_2m,relative_humidity_2m,dew_point_2m"
      ",apparent_temperature,precipitation_probability,precipitation"
      ",weather_code,cloud_cover,pressure_msl"
      ",wind_speed_10m,wind_direction_10m,wind_gusts_10m"
      ",uv_index,visibility,is_day"
    + "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      ",sunrise,sunset,uv_index_max,precipitation_sum"
      ",precipitation_probability_max"
      ",wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant";

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + OPENMETEO_ENDPOINT + uri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3)
  {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED)
    {
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.useHTTP10(true);
    http.begin(client, OPENMETEO_ENDPOINT, OPENMETEO_PORT, uri);
    http.addHeader("Accept-Encoding", "identity");
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK)
    {
      jsonErr = deserializeForecast(http.getStream(), r);
      if (jsonErr)
      {
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " "
                   + getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
} // getOpenMeteoForecast

/* Perform an HTTPS GET request to Open-Meteo's Air Quality API.
 * Fetches the last NUM_AIR_POLLUTION hours of pollutant data.
 * Data is parsed and stored in r.
 *
 * Returns the HTTP Status Code.
 */
#ifdef USE_HTTP
  int getOpenMeteoAirQuality(WiFiClient &client, weather_air_quality_t &r)
#else
  int getOpenMeteoAirQuality(WiFiClientSecure &client, weather_air_quality_t &r)
#endif
{
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};

  // past_hours=24 + forecast_hours=1 gives NUM_AIR_POLLUTION (24) entries
  // covering the last 24 hours up to the current hour.
  String uri = "/v1/air-quality?latitude=" + LAT + "&longitude=" + LON
    + "&timeformat=unixtime"
    + "&past_hours=" + String(NUM_AIR_POLLUTION)
    + "&forecast_hours=1"
    + "&hourly=carbon_monoxide,nitrogen_dioxide,sulphur_dioxide"
      ",ozone,pm2_5,pm10,ammonia";

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + OPENMETEO_AQ_ENDPOINT + uri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3)
  {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED)
    {
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.useHTTP10(true);
    http.begin(client, OPENMETEO_AQ_ENDPOINT, OPENMETEO_PORT, uri);
    http.addHeader("Accept-Encoding", "identity");
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK)
    {
      jsonErr = deserializeAirQuality(http.getStream(), r);
      if (jsonErr)
      {
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " "
                   + getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
} // getOpenMeteoAirQuality

/* Fetch waste collection schedule from sbm.jumomind.com for the current and
 * next month. Results are cached in NVS for the entire calendar month so the
 * API is called at most once per month.
 *
 * Fills wc.next with the next upcoming pickup (today or later) and wc.days
 * with the number of days until that pickup (0 = today).
 *
 * Returns HTTP_CODE_OK (200) on success or a negative error code.
 */
int getWasteCollection(waste_collection_t &wc, const tm *timeInfo)
{
  wc.valid = false;

  int curYear  = timeInfo->tm_year + 1900;
  int curMonth = timeInfo->tm_mon + 1; // 1-12

  // --- NVS cache check ---
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  int cachedYear  = prefs.getInt("wcYear",  0);
  int cachedMonth = prefs.getInt("wcMonth", 0);
  String jsonStr  = prefs.getString("wcJson", "");
  prefs.end();

  bool cacheHit = (cachedYear == curYear && cachedMonth == curMonth
                   && jsonStr.length() > 4);

  if (!cacheHit)
  {
    // Fetch current month and next month so we always have dates past the
    // month boundary.
    int nextMonth = curMonth + 1;
    int nextYear  = curYear;
    if (nextMonth > 12) { nextMonth = 1; nextYear++; }

    auto buildUrl = [](int y, int m) -> String {
      char buf[8];
      snprintf(buf, sizeof(buf), "%04d-%02d", y, m);
      return String("https://sbm.jumomind.com/mmapp/api.php?r=calendar/")
             + buf + "&city_id=" + WASTE_CITY_ID + "&area_id=" + WASTE_AREA_ID;
    };

    WiFiClientSecure wcClient;
    wcClient.setInsecure(); // no cert pinning for this community API
    HTTPClient http;

    // Helper: fetch one month and append its "dates" array to a JSON array string
    String mergedDates = "[";
    bool firstEntry = true;
    int lastHttpCode = 0;

    for (int pass = 0; pass < 2; ++pass)
    {
      String url = (pass == 0) ? buildUrl(curYear, curMonth)
                               : buildUrl(nextYear, nextMonth);
      Serial.print("[waste] GET " + url);
      http.begin(wcClient, url);
      http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);
      lastHttpCode = http.GET();
      Serial.println(" -> " + String(lastHttpCode));

      if (lastHttpCode == HTTP_CODE_OK)
      {
        String body = http.getString();
        // Extract the "dates" array from {"year":...,"month":...,"dates":[...]}
        int start = body.indexOf("\"dates\":[");
        if (start >= 0)
        {
          start += 8; // points to '['
          int depth = 0, end = start;
          for (; end < (int)body.length(); ++end)
          {
            if (body[end] == '[') depth++;
            else if (body[end] == ']') { depth--; if (depth == 0) { end++; break; } }
          }
          String arr = body.substring(start, end); // "[{...},{...}]"
          // Strip surrounding brackets and append entries
          String inner = arr.substring(1, arr.length() - 1);
          inner.trim();
          if (inner.length() > 0)
          {
            if (!firstEntry) mergedDates += ",";
            mergedDates += inner;
            firstEntry = false;
          }
        }
      }
      http.end();
    }
    mergedDates += "]";

    if (lastHttpCode != HTTP_CODE_OK && firstEntry)
    {
      return lastHttpCode; // both months failed
    }

    jsonStr = mergedDates;

    // Persist to NVS
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("wcYear",  curYear);
    prefs.putInt("wcMonth", curMonth);
    prefs.putString("wcJson", jsonStr);
    prefs.end();
    Serial.println("[waste] cached " + String(jsonStr.length()) + " bytes");
  }
  else
  {
    Serial.println("[waste] NVS cache hit (" + String(jsonStr.length()) + " bytes)");
  }

  // --- Parse and find the next upcoming pickup ---
  // Build today's date string "YYYY-MM-DD" for lexicographic comparison
  char todayBuf[11];
  snprintf(todayBuf, sizeof(todayBuf), "%04d-%02d-%02d",
           curYear, curMonth, timeInfo->tm_mday);
  String today = String(todayBuf);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err)
  {
    Serial.println("[waste] JSON parse error: " + String(err.c_str()));
    return -256;
  }

  JsonArray arr = doc.as<JsonArray>();
  String bestDay   = "";
  String bestTitle = "";
  String bestColor = "";

  for (JsonObject item : arr)
  {
    String day = item["day"] | "";
    if (day >= today && (bestDay.isEmpty() || day < bestDay))
    {
      bestDay   = day;
      bestTitle = item["title"] | "";
      bestColor = item["color"] | "";
    }
  }

  if (bestDay.isEmpty())
  {
    Serial.println("[waste] no upcoming pickup found");
    return HTTP_CODE_OK; // valid fetch, just nothing upcoming this month
  }

  wc.next.day   = bestDay;
  wc.next.title = bestTitle;
  wc.next.color = bestColor;

  // Compute days until pickup (simple date arithmetic via mktime)
  struct tm pickupTm = *timeInfo;
  pickupTm.tm_year = bestDay.substring(0, 4).toInt() - 1900;
  pickupTm.tm_mon  = bestDay.substring(5, 7).toInt() - 1;
  pickupTm.tm_mday = bestDay.substring(8, 10).toInt();
  pickupTm.tm_hour = 0; pickupTm.tm_min = 0; pickupTm.tm_sec = 0;
  struct tm nowTm = *timeInfo;
  nowTm.tm_hour = 0; nowTm.tm_min = 0; nowTm.tm_sec = 0;
  time_t tPickup = mktime(&pickupTm);
  time_t tNow    = mktime(&nowTm);
  wc.days  = (int)((tPickup - tNow) / 86400);
  wc.valid = true;

  Serial.println("[waste] next: " + bestTitle + " on " + bestDay
                 + " (in " + wc.days + " days)");
  return HTTP_CODE_OK;
} // getWasteCollection

/* Prints debug information about heap usage.
 */
void printHeapUsage() {
  Serial.println("[debug] Heap Size       : "
                 + String(ESP.getHeapSize()) + " B");
  Serial.println("[debug] Available Heap  : "
                 + String(ESP.getFreeHeap()) + " B");
  Serial.println("[debug] Min Free Heap   : "
                 + String(ESP.getMinFreeHeap()) + " B");
  Serial.println("[debug] Max Allocatable : "
                 + String(ESP.getMaxAllocHeap()) + " B");
  return;
}

