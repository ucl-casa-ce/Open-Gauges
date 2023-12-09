from neopixel import Neopixel
from mqtt_as import MQTTClient, config
from config import wifi_led, blue_led
import uasyncio as asyncio
import machine
import ntptime
import time

# Set up NeoPixels
numpix = 144
pixels = Neopixel(numpix, 0, 15, "GRB")

colors = {
    'BLUE': (0, 0, 255),
    'GREEN': (0, 255, 0),
    'YELLOW': (255, 100, 0),
    'ORANGE': (255, 50, 0),
    'RED': (255, 0, 0),
    'OFF': (0, 0, 0)
}

pixels.brightness(255)
prev_wind = 0
prev_max_wind = 0
max_wind = 0  # Initialize max_wind

WIND_SPEED_RANGE = [0, 60] #actual is half this amount to allow the max wind marker virtually
multiplier = numpix / (WIND_SPEED_RANGE[1] - WIND_SPEED_RANGE[0])

UPDATE_THRESHOLD = 2  # Only update if wind speed changes by 2 or more

def set_pixel_color(wind, max_wind):
    global prev_wind
    
    if abs(wind - prev_wind) < UPDATE_THRESHOLD:
        return

    while prev_wind < wind:
        update_pixels(prev_wind)
        time.sleep(0.1)
        prev_wind += 1

    while prev_wind > wind:
        update_pixels(prev_wind)
        time.sleep(0.1)
        prev_wind -= 1

    pixels[max_wind] = colors['RED']  # Update max_wind pixel

    pixels.show()


def update_pixels(wind_value):
    for i in range(1, numpix):
        if i <= wind_value < numpix:
            if i <= 20:
                color = colors['BLUE']
            elif i <= 40:
                color = colors['GREEN']
            elif i <= 80:
                color = colors['YELLOW']
            elif i <= 100:
                color = colors['ORANGE']
            else:
                color = colors['RED']
            pixels[i] = color
        elif i == max_wind:  # Use max_wind directly
            pixels[i] = colors['RED']
        else:
            pixels[i] = colors['OFF']
    pixels.show()

def sub_cb(topic, msg, retained):
    global max_wind, prev_max_wind

    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    wind_speed = float(msg)

    if WIND_SPEED_RANGE[0] <= wind_speed <= WIND_SPEED_RANGE[1]:
        wind = (wind_speed - WIND_SPEED_RANGE[0]) * multiplier

        if wind > max_wind:
            max_wind = int(wind)

        if max_wind != prev_max_wind:  # Check if max_wind has changed
            print("Max Wind", max_wind)
            prev_max_wind = max_wind

        set_pixel_color(wind, max_wind)
    else:
        for i in range(numpix):
            pixels[i] = colors['OFF']
        pixels.show()


async def get_current_minute():
    try:
        ntptime.settime()  
        current_time = time.localtime()
        return current_time[4]  
    except:
        print("Could not get the time from the internet")
        return None

#Reset Wind Max and System at Midnight
async def reset_on_hour(): 
    while True:
        try:
            ntptime.settime()  # Get the current time from the internet
            current_time = time.localtime()
            if current_time[3] == 0 and current_time[4] == 0:  # Check if hour and minute are both 0
                machine.reset()
            else:
                await asyncio.sleep(60 - current_time[5])
        except:
            print("Could not get the time from the internet")
            await asyncio.sleep(60)  # Retry after 1 minute if time sync fails


async def heartbeat():
    s = True
    while True:
        await asyncio.sleep_ms(500)
        blue_led(s)
        s = not s

async def wifi_han(state):
    wifi_led(not state)
    print('Wifi is ', 'up' if state else 'down')
    await asyncio.sleep(1)
    if state:
        asyncio.create_task(reset_on_hour())  # Start reset_on_hour task after WiFi is connected

async def conn_han(client):
     # await client.subscribe('personal/ucfnaps/saber/config/', 1)
       await client.subscribe('personal/ucfnaps/downhamweather/windSpeed_mph', 1)

async def main(client):
    try:
        await client.connect()  # Ensure WiFi and MQTT connection before starting other tasks
    except OSError:
        print('Connection failed.')
        return
    
    n = 0
    while True:
        await asyncio.sleep(5)
        n += 1

# Define configuration
config['subs_cb'] = sub_cb
config['wifi_coro'] = wifi_han
config['connect_coro'] = conn_han
config['clean'] = True

# Set up client
MQTTClient.DEBUG = True  
client = MQTTClient(config)

asyncio.create_task(heartbeat())

try:
    asyncio.run(main(client))
finally:
    client.close()  
    asyncio.new_event_loop()
