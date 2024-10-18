import json
import time
import requests
import neopixel
import board

# Set Globals - baro, open weather map code & to check if the connection has run
baro = 1050
code = 800
run = 0
trend = "steady"

# Set list for 3 hours (every 5 min updates to determine trend)
barolist = [0]*37  # Initialize list with 37 zeros

# Set Colours
DRY = (255, 0, 0)
YELLOW = (255, 150, 0)
FAIR = (100, 64, 0)
CHANGE = (0, 255, 165)
CYAN = (0, 255, 255)
STORM = (0, 0, 139)
RAIN = (65, 105, 225)
SNOW_COLOR = (255, 255, 255)  # Renamed to avoid conflict with 'SNOW' list

FALLING = (255, 0, 0)
RISING = (0, 0, 225)
STEADY = (0, 255, 0)

SUNNY = (255, 255, 0)
CLOUDY = (0, 255, 255)
PARTLYCLOUDY = (0, 255, 255)
NIGHTCLEAR = (0, 255, 255)
THUNDERSTORM_COLOR = (255, 150, 0)  # Renamed to avoid conflict
FOGGY = (125, 125, 125)

RED = (255, 0, 0)
GREEN = (0, 255, 0)

OFF = (0, 0, 0)

# Set NeoPixel Details - Pin/Number Pixels/Brightness etc
pixels = neopixel.NeoPixel(board.D18, 144, brightness=0.03, auto_write=False)

print("Running Start Up")

# Start up Lights
n = 1
t_end = time.time() + 22.32 * 1
while time.time() < t_end:
    n = n + 1
    if n >= 144:
        n = 1
    pixels[n] = SUNNY
    pixels[n-1] = STORM
    pixels.show()
    time.sleep(0.1)

pixels.fill(OFF)
pixels.show()

# Set OpenWeatherMap API
def getconditions():
    global run, baro, code, barolist  # Added barolist here
    try:
        gettingdata()
        print("Getting Data")
        api_key = "YOUR_API_KEY"  # Replace with your actual API key
        lat = YOUR_LATITUDE       # Replace with your actual latitude (e.g., 33.44)
        lon = YOUR_LONGITUDE      # Replace with your actual longitude (e.g., -94.04)

        # Ensure 'current' data is included
        url = f"https://api.openweathermap.org/data/3.0/onecall?lat={lat}&lon={lon}&exclude=minutely,hourly,daily,alerts&units=metric&appid={api_key}"

        response = requests.get(url)
        if response.status_code == 200:
            data = response.json()
            # Print the data to inspect its structure (optional)
            # print(json.dumps(data, indent=4))

            if 'current' in data:
                baro = int(data["current"]["pressure"])
                print(f"Pressure: {baro} hPa")
                code = int(data["current"]["weather"][0]["id"])
                print(f"Weather Code: {code}")
                barolist.append(baro)
                barolist = barolist[-36:]
                run = 1
            else:
                print("The 'current' key is not present in the API response.")
                run = 0
        else:
            print(f"Error fetching data: {response.status_code}")
            run = 0
            time.sleep(120)
    except requests.exceptions.RequestException as e:
        pixels[0] = RED
        pixels.show()
        print("Error on Data:", e)
        run = 0
        time.sleep(120)
    except KeyError as e:
        print(f"KeyError: {e} key not found in the API response.")
        run = 0
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        run = 0

# Set Barometer Pixel
def barometer():
    # Barometer pixels from 43 to 143 - 950Mb to 1050Mb, Each Pixel Editable
    for i in range(43, 144):
        baro_value = 950 + (i - 43)
        if baro >= baro_value and baro < 1060:
            if baro_value < 973:
                pixels[i] = STORM
            elif baro_value < 990:
                pixels[i] = RAIN
            elif baro_value < 1006:
                pixels[i] = CHANGE
            elif baro_value < 1022:
                pixels[i] = FAIR
            else:
                pixels[i] = DRY
        else:
            pixels[i] = OFF

def current_weather():
    global code
    # Code Ranges
    scatteredclouds = [802, 803]
    showers = [521, 522, 531]
    heavyrain = [502, 503, 504]
    thunderstorm_codes = [200, 201, 202, 210, 211, 212, 221, 230, 231, 232]
    snow_codes = [600, 601, 602, 620, 622]
    sleet = [611, 615]
    fog = [701, 741]

    decoded = ""

    # Clear previous weather pixels
    weather_pixels = [25, 27, 30, 34, 36]
    for pixel in weather_pixels:
        pixels[pixel] = OFF

    if code == 800:
        decoded = "Sunny"
        pixels[36] = SUNNY

    elif code == 801:
        decoded = "Few Clouds"
        pixels[34] = CLOUDY

    elif code in scatteredclouds:
        decoded = "Scattered Clouds"
        pixels[36] = CLOUDY

    elif code == 804:
        decoded = "Overcast"
        pixels[36] = CLOUDY

    elif code in showers or code == 500 or code == 501:
        decoded = "Rain"
        pixels[30] = RAIN

    elif code in heavyrain:
        decoded = "Heavy Rain"
        pixels[30] = RAIN

    elif code in thunderstorm_codes:
        decoded = "Thunderstorms"
        pixels[27] = THUNDERSTORM_COLOR

    elif code in snow_codes or code in sleet:
        decoded = "Snow"
        pixels[25] = SNOW_COLOR

    elif code in fog:
        decoded = "Mist / Fog"
        pixels[25] = FOGGY

    else:
        decoded = "Unknown Weather"

    print(decoded)

def trendbasic():
    global barolist  # Declare as global if you modify it
    print(barolist)

    trend_value = barolist[-1] - barolist[-30]
    print("Trend =", trend_value)

    # Clear trend pixels
    trend_pixels = [8, 9, 10, 18, 19, 20, 27, 28, 29, 30]
    for pixel in trend_pixels:
        pixels[pixel] = OFF

    if -0.25 <= trend_value <= 0.25:
        pixels[18] = STEADY
        pixels[19] = STEADY
        pixels[20] = STEADY

    elif trend_value > 0.25:
        pixels[27] = RISING
        pixels[28] = RISING
        pixels[29] = RISING
        pixels[30] = RISING

    elif trend_value < -0.25:
        pixels[8] = FALLING
        pixels[9] = FALLING
        pixels[10] = FALLING

def gettingdata():
    pixels[0] = GREEN
    pixels.show()
    time.sleep(2)
    pixels[0] = OFF
    pixels.show()
    time.sleep(2)
    pixels[0] = GREEN
    pixels.show()
    time.sleep(2)

while True:
    try:
        getconditions()
        barometer()
        current_weather()
        trendbasic()
        pixels.show()
        print("Data Successful, Now Sleeping")
        time.sleep(300)
    except Exception as e:
        print("Data Error, Now Sleeping, Trying Again in 5 Minutes")
        print(e)
        time.sleep(300)
