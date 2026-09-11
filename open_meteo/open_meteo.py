# Component to fetch Open Meteo weather data and publish the data locally to other components.

import sys
from pathlib import Path

def add_schema_path():
    # Determine starting directory
    if "__file__" in globals():
        start = Path(__file__).resolve().parent
    else:
        # interactive python
        start = Path.cwd()

    for p in [start] + list(start.parents):
        candidate = p / "build" / "schema"
        if candidate.exists():
            sys.path.insert(0, str(candidate))
            return candidate

    raise RuntimeError("Could not locate build/schema directory")

sys.path.append("/usr/lib/python3.8/site-packages")
add_schema_path()

import time
import signal
import random
import os
import argparse
import flatbuffers
import fabrix
import openmeteo_requests
import requests_cache
from retry_requests import retry
from datetime import datetime, timezone
from fabrix import rcu

import CEMS.OpenMeteo.WeatherCurrent as WeatherCurrent
import CEMS.OpenMeteo.WeatherForecast as WeatherForecast
import CEMS.OpenMeteo.HourlyForecast as HourlyForecast

# ================== SIGNAL HANDLING ==================

exit_code = 0
stop = False

# Signal handler to handle Ctrl+C
def interrupt_handler(signum, frame):
    global stop
    if signum == signal.SIGINT or signum == signal.SIGTERM:
        stop = True

class WeatherProvider:
    def __init__(self, lat=50.92, lon=4.75):
        self.lat = lat
        self.lon = lon

        # Setup the Open-Meteo API client with internal sqlite cache and session retry rules
        cache_session = requests_cache.CachedSession('.cache', expire_after=3600)
        retry_session = retry(cache_session, retries=5, backoff_factor=0.2)
        self.client = openmeteo_requests.Client(session=retry_session)

        self.url = "https://api.open-meteo.com/v1/forecast"
        self.last_fetch_ts = 0.0
        self.fetch_interval = 3600.0  # Fetch once per hour
        self.cached_forecast = {}
        self.response_latitude = self.lat
        self.response_longitude = self.lon
        self.response_timezone_abbreviation = ""
        self.response_utc_offset_second = 0
        self.response_elevation_m_asl = 0

        self.params = {
            "latitude": self.lat,
            "longitude": self.lon,
            "hourly": [
                "temperature_2m", 
                "direct_normal_irradiance", 
                "diffuse_radiation", 
                "direct_radiation", 
                "shortwave_radiation", 
                "relative_humidity_2m", 
                "precipitation_probability", 
                "precipitation", 
                "pressure_msl", 
                "cloud_cover_low", 
                "cloud_cover_mid", 
                "cloud_cover_high", 
                "visibility", 
                "wind_speed_10m",
                "cloud_cover",
                "rain",
                "showers",
                "snowfall",
                "dew_point_2m",
                "weather_code"
            ],
            "forecast_days": 2,
            "timezone": "Europe/Berlin"
        }

    def update_cache(self):
        """Executes an SDK lookup or throttles via time check."""
        now = time.time()
        if now - self.last_fetch_ts < self.fetch_interval and self.cached_forecast:
            return

        try:
            responses = self.client.weather_api(self.url, params=self.params)
            response = responses[0]
            self.response_latitude = response.Latitude()
            self.response_longitude = response.Longitude()
            self.response_timezone_abbreviation = response.TimezoneAbbreviation()
            self.response_utc_offset_second = response.UtcOffsetSeconds()
            self.response_elevation_m_asl = response.Elevation()
            hourly = response.Hourly()

            # Extract raw continuous memory segments into numpy arrays matching requested order
            temp_vec = hourly.Variables(0).ValuesAsNumpy()
            direct_normal_vec = hourly.Variables(1).ValuesAsNumpy()
            diffuse_vec = hourly.Variables(2).ValuesAsNumpy()
            direct_rad_vec = hourly.Variables(3).ValuesAsNumpy()
            shortwave_vec = hourly.Variables(4).ValuesAsNumpy()
            rh_vec = hourly.Variables(5).ValuesAsNumpy()
            precip_prob_vec = hourly.Variables(6).ValuesAsNumpy()
            precip_vec = hourly.Variables(7).ValuesAsNumpy()
            pressure_vec = hourly.Variables(8).ValuesAsNumpy()
            cloud_low_vec = hourly.Variables(9).ValuesAsNumpy()
            cloud_mid_vec = hourly.Variables(10).ValuesAsNumpy()
            cloud_high_vec = hourly.Variables(11).ValuesAsNumpy()
            visibility_vec = hourly.Variables(12).ValuesAsNumpy()
            wind_vec = hourly.Variables(13).ValuesAsNumpy()
            cloud_cover_vec = hourly.Variables(14).ValuesAsNumpy()
            rain_vec = hourly.Variables(15).ValuesAsNumpy()
            showers_vec = hourly.Variables(16).ValuesAsNumpy()
            snowfall_vec = hourly.Variables(17).ValuesAsNumpy()
            dew_point_2m_vec = hourly.Variables(18).ValuesAsNumpy()
            weather_code_vec = hourly.Variables(19).ValuesAsNumpy()

            start_epoch = hourly.Time()
            num_elements = len(temp_vec)
            epochs = [
                start_epoch + i * 3600
                for i in range(num_elements)
            ]

            self.cached_forecast = {
                "epochs": epochs,
                "temperature": temp_vec.tolist(),
                "direct_normal_irradiance": direct_normal_vec.tolist(),
                "diffuse_horizontal_irradiance": diffuse_vec.tolist(),
                "direct_radiation": direct_rad_vec.tolist(),
                "shortwave_radiation": shortwave_vec.tolist(),
                "relative_humidity": rh_vec.tolist(),
                "precipitation_probability": precip_prob_vec.tolist(),
                "precipitation": precip_vec.tolist(),
                "pressure_msl": pressure_vec.tolist(),
                "cloud_cover_low": cloud_low_vec.tolist(),
                "cloud_cover_mid": cloud_mid_vec.tolist(),
                "cloud_cover_high": cloud_high_vec.tolist(),
                "visibility": visibility_vec.tolist(),
                "wind_speed": wind_vec.tolist(),
                "cloud_cover": cloud_cover_vec.tolist(),
                "rain": rain_vec.tolist(),
                "showers": showers_vec.tolist(),
                "snowfall": snowfall_vec.tolist(),
                "dew_point_2m": dew_point_2m_vec.tolist(),
                "weather_code": weather_code_vec.tolist()
            }
            self.last_fetch_ts = now
            print("[Weather] Forecast cache synchronized via Open-Meteo SDK.")
        except Exception as e:
            print(f"\033[93m[Weather Warning] SDK invocation failed: {e}. Utilizing fallback cache.\033[0m")

    def get_current_features(self):
        """Resolves the current hour's weather metrics. Falls back to neutral vectors on failure."""
        self.update_cache()
        if not self.cached_forecast:
            return None

        now_epoch = int(time.time())
        epochs = self.cached_forecast["epochs"]
        closest_idx = min(range(len(epochs)), key=lambda i: abs(epochs[i] - now_epoch))

        return {key: self.cached_forecast[key][closest_idx] for key in self.cached_forecast if key != "epochs"}

    def get_full_forecast_sequence(self):
        """Extracts the entire multi-hour timeline data matrix for sequence streaming."""
        self.update_cache()
        if not self.cached_forecast:
            return []

        sequences = []
        epochs = self.cached_forecast["epochs"]
        for idx in range(len(epochs)):
            sequences.append({
                "timestamp": float(epochs[idx]),
                "temp": self.cached_forecast["temperature"][idx],
                "direct_normal": self.cached_forecast["direct_normal_irradiance"][idx],
                "diffuse": self.cached_forecast["diffuse_horizontal_irradiance"][idx],
                "direct_rad": self.cached_forecast["direct_radiation"][idx],
                "shortwave": self.cached_forecast["shortwave_radiation"][idx],
                "relative_humidity": self.cached_forecast["relative_humidity"][idx],
                "precipitation_probability": self.cached_forecast["precipitation_probability"][idx],
                "precipitation": self.cached_forecast["precipitation"][idx],
                "pressure_msl": self.cached_forecast["pressure_msl"][idx],
                "cloud_low": self.cached_forecast["cloud_cover_low"][idx],
                "cloud_mid": self.cached_forecast["cloud_cover_mid"][idx],
                "cloud_high": self.cached_forecast["cloud_cover_high"][idx],
                "visibility": self.cached_forecast["visibility"][idx],
                "wind": self.cached_forecast["wind_speed"][idx],
                "cloud_cover": self.cached_forecast["cloud_cover"][idx],
                "rain": self.cached_forecast["rain"][idx],
                "showers": self.cached_forecast["showers"][idx],
                "snowfall": self.cached_forecast["snowfall"][idx],
                "dew_point_2m": self.cached_forecast["dew_point_2m"][idx],
                "weather_code": self.cached_forecast["weather_code"][idx]
            })
        return sequences

# ================== COMPONENT ==================

class OpenMeteo(fabrix.Component):

    _TOPIC_NAME_WEATHER_CURRENT = "WeatherCurrent"
    _TOPIC_NAME_WEATHER_FORECAST = "WeatherForecast"

    def __init__(self, *args, **kwargs):
        self._verbose = kwargs.pop("verbose", False)
        self.lat = kwargs.pop("lat", 50.92)
        self.lon = kwargs.pop("lon", 4.75)
        super().__init__(*args, **kwargs)

        # Create live current slice tracking area
        self.current_area = rcu.create_area(self.public_endpoint(), self._TOPIC_NAME_WEATHER_CURRENT)
        self.current_area.grace_period(2)

        # Create downstream multi-hour sequence array projection area
        self.forecast_area = rcu.create_area(self.public_endpoint(), self._TOPIC_NAME_WEATHER_FORECAST)
        self.forecast_area.grace_period(2)

        self.weather = WeatherProvider(lat=self.lat, lon=self.lon)

    def run(self):
        """Main loop."""
        timestep = 1.0
        next_timepoint = time.time() + timestep
        counter = 0
        while not stop:
            self.process_until(next_timepoint)
            if counter == 0:
                self._act()
            counter += 1
            if counter == 60: counter = 0
            while next_timepoint <= time.time(): next_timepoint += timestep 

    def _on_error(self, other_end, error_code):
        print(f"Error: {other_end.identifier().name() if other_end else '<>'} with error code {fabrix.EnumNameErrorCode(error_code)}")

    def _on_start(self):
        print(f"Component {self.identifier().name()} is online with pid {os.getpid()}.")

    def _on_subscribe_request(self, sender_endpoint, delivery_endpoint, topic_name):
        if topic_name == self._TOPIC_NAME_WEATHER_CURRENT or topic_name == self._TOPIC_NAME_WEATHER_FORECAST: return True
        return False

    def _on_unsubscribe_request(self, sender_endpoint, delivery_endpoint, topic_name):
        if topic_name == self._TOPIC_NAME_WEATHER_CURRENT or topic_name == self._TOPIC_NAME_WEATHER_FORECAST: return True
        return False

    def _on_list_topics_request(self, sender_endpoint, topics):
        topics.append(self._TOPIC_NAME_WEATHER_CURRENT)
        topics.append(self._TOPIC_NAME_WEATHER_FORECAST)

    def _act(self):
        # 1. Capture and Publish Current Scalar State
        metrics = self.weather.get_current_features()
        if metrics == None:
            self.current_area.publish_none()
            return

        builder = flatbuffers.Builder(1024)
        abbr_offset = builder.CreateString(self.weather.response_timezone_abbreviation)
        WeatherCurrent.Start(builder)
        WeatherCurrent.AddTimestamp(builder, self.weather.last_fetch_ts)
        WeatherCurrent.AddLatitude(builder, self.weather.response_latitude)
        WeatherCurrent.AddLongitude(builder, self.weather.response_longitude)
        WeatherCurrent.AddElevation(builder, self.weather.response_elevation_m_asl)
        WeatherCurrent.AddTimezoneAbbreviation(builder, abbr_offset)
        WeatherCurrent.AddUtcOffsetSecond(builder, self.weather.response_utc_offset_second)
        WeatherCurrent.AddTemperature(builder, metrics["temperature"])
        WeatherCurrent.AddDirectNormalIrradiance(builder, metrics["direct_normal_irradiance"])
        WeatherCurrent.AddDiffuseHorizontalIrradiance(builder, metrics["diffuse_horizontal_irradiance"])
        WeatherCurrent.AddDirectRadiation(builder, metrics["direct_radiation"])
        WeatherCurrent.AddShortwaveRadiation(builder, metrics["shortwave_radiation"])
        WeatherCurrent.AddRelativeHumidity(builder, float(metrics["relative_humidity"]))
        WeatherCurrent.AddPrecipitationProbability(builder, float(metrics["precipitation_probability"]))
        WeatherCurrent.AddPrecipitation(builder, metrics["precipitation"])
        WeatherCurrent.AddPressureMsl(builder, metrics["pressure_msl"])
        WeatherCurrent.AddCloudCoverLow(builder, float(metrics["cloud_cover_low"]))
        WeatherCurrent.AddCloudCoverMid(builder, float(metrics["cloud_cover_mid"]))
        WeatherCurrent.AddCloudCoverHigh(builder, float(metrics["cloud_cover_high"]))
        WeatherCurrent.AddVisibility(builder, metrics["visibility"])
        WeatherCurrent.AddWindSpeed(builder, metrics["wind_speed"])
        WeatherCurrent.AddCloudCover(builder, float(metrics["cloud_cover"]))
        WeatherCurrent.AddRain(builder, metrics["rain"])
        WeatherCurrent.AddShowers(builder, metrics["showers"])
        WeatherCurrent.AddSnowfall(builder, metrics["snowfall"])
        WeatherCurrent.AddDewPoint2m(builder, metrics["dew_point_2m"])
        WeatherCurrent.AddWeatherCode(builder, int(metrics["weather_code"]))
        offset = WeatherCurrent.End(builder)
        builder.Finish(offset)

        self.broadcast_topic(self._TOPIC_NAME_WEATHER_CURRENT, builder.Output())
        self.current_area.publish_storage(self.current_area.create_storage(builder.Output()))
        self.current_area.tick()
        self.current_area.reclaim()

        # 2. Capture and Publish Complete Forward Forecast Timeline Array
        timeline = self.weather.get_full_forecast_sequence()
        if timeline:
            fc_builder = flatbuffers.Builder(4096)

            # FlatBuffers requires struct vector payloads packed in reverse order
            WeatherForecast.StartHourlyVector(fc_builder, len(timeline))
            for frame in reversed(timeline):
                HourlyForecast.CreateHourlyForecast(
                    fc_builder,
                    frame["timestamp"],
                    int(frame["weather_code"]),
                    frame["temp"],
                    frame["direct_normal"],
                    frame["diffuse"],
                    frame["direct_rad"],
                    frame["shortwave"],
                    frame["relative_humidity"],
                    frame["precipitation_probability"],
                    frame["precipitation"],
                    frame["pressure_msl"],
                    frame["cloud_low"],
                    frame["cloud_mid"],
                    frame["cloud_high"],
                    frame["visibility"],
                    frame["wind"],
                    frame["cloud_cover"],
                    frame["rain"],
                    frame["showers"],
                    frame["snowfall"],
                    frame["dew_point_2m"]   
                )
            vector_offset = fc_builder.EndVector()

            WeatherForecast.Start(fc_builder)
            WeatherForecast.AddGeneratedTimestamp(fc_builder, self.weather.last_fetch_ts)
            WeatherForecast.AddLatitude(fc_builder, self.weather.response_latitude)
            WeatherForecast.AddLongitude(fc_builder, self.weather.response_longitude)
            WeatherForecast.AddHourly(fc_builder, vector_offset)
            fc_offset = WeatherForecast.End(fc_builder)
            fc_builder.Finish(fc_offset)

            self.broadcast_topic(self._TOPIC_NAME_WEATHER_FORECAST, fc_builder.Output())
            self.forecast_area.publish_storage(self.forecast_area.create_storage(fc_builder.Output()))
            self.forecast_area.tick()
            self.forecast_area.reclaim()

# ================== MAIN ==================

def parse_args():
    parser = argparse.ArgumentParser(description="Open Meteo Weather Forecast")

    parser.add_argument("-n", "--name", required=True,
                        help="Component name")

    parser.add_argument("-r", "--realm", default="cems",
                        help="Realm name (default: cems)")

    parser.add_argument("--lat", type=float, default=50.92,
                        help="Latitude coordinate (default: 50.92)")

    parser.add_argument("--lon", type=float, default=4.75,
                        help="Longitude coordinate (default: 4.75)")

    parser.add_argument("--verbose", action="store_true",
                        help="Enable verbose logging")

    return parser.parse_args()

def main():
    """Main function"""
    global exit_code

    args = parse_args()

    # Random seed
    random.seed()

    # Register 'break' handler
    signal.signal(signal.SIGINT, interrupt_handler)
    signal.signal(signal.SIGTERM, interrupt_handler)

    # Create component passing coordinates via kwargs
    comp = OpenMeteo(
        args.name,
        args.realm,
        65535,
        lat=args.lat,
        lon=args.lon,
        verbose=args.verbose,
    )

    # Loop
    comp.run()

    # Cleanup
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGTERM, signal.SIG_DFL)

    return exit_code

if __name__ == "__main__":
    sys.exit(main())
