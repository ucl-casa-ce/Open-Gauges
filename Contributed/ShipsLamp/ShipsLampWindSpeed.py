from neopixel import Neopixel
from mqtt_as import MQTTClient, config
from config import wifi_led, blue_led
import uasyncio as asyncio
import machine
import random
import time

# --- Set up NeoPixels ---
numpix = 60
pixels = Neopixel(numpix, 0, 15, "GRB")
OFF = (0, 0, 0)
pixels.brightness(100)
current_color = OFF

# --- "Weather Map" Color Blending Function ---
def blend_color(wind_speed):
    """
    Calculates a "weather map" gradient:
    0 = OFF, 0-10 = Green, 10-20 = Green->Yellow,
    20-30 = Yellow->Orange, 30-40 = Orange->Red, >40 = Red
    """
    if wind_speed <= 0:
        return OFF  # (0, 0, 0)
    if wind_speed >= 40:
        return (255, 0, 0) # Solid Red
    if wind_speed < 10:
        return (0, 255, 0) # Solid Green
    if wind_speed < 20:
        t = (wind_speed - 10) / 10.0
        r = int(255 * t)
        g = 255
        b = 0
        return (r, g, b)
    if wind_speed < 30:
        t = (wind_speed - 20) / 10.0
        r = 255
        g = int(255 * (1.0 - t) + 127 * t)
        b = 0
        return (r, g, b)
    else: # 30-40
        t = (wind_speed - 30) / 10.0
        r = 255
        g = int(127 * (1.0 - t))
        b = 0
        return (r, g, b)

# --- Combined Flame Effect Task ---
async def flame_effect():
    global current_color
    while True:
        # --- STEADY PHASE ---
        steady_time_ms = random.randint(10000, 60000)
        start_time = time.ticks_ms()
        steady_color = current_color
        pixels.fill(steady_color)
        pixels.show()

        while time.ticks_diff(time.ticks_ms(), start_time) < steady_time_ms:
            if current_color != steady_color:
                steady_color = current_color
                pixels.fill(steady_color)
                pixels.show()
            await asyncio.sleep_ms(200)

        # --- FLICKER PHASE ---
        if current_color != OFF:
            flicker_duration_ms = random.randint(5000, 15000)
            start_time = time.ticks_ms()

            while time.ticks_diff(time.ticks_ms(), start_time) < flicker_duration_ms:
                base_color = current_color
                reduction = random.randint(40, 120)
                scale = max(0, (255 - reduction)) / 255.0
                r = int(base_color[0] * scale)
                g = int(base_color[1] * scale)
                b = int(base_color[2] * scale)
                pixels.fill((r, g, b))
                pixels.show()
                await asyncio.sleep_ms(random.randint(50, 150))

# --- Timer Task ---
# REMOVED! We are using the hardware watchdog instead.

# --- MQTT Callback ---
def sub_cb(topic, msg, retained):
    global current_color
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    try:
        wind = float(msg)
        print(wind)
        current_color = blend_color(wind)
    except ValueError:
        print("Received non-float message")

# --- Heartbeat Task (NOW WITH WATCHDOG) ---
async def heartbeat():
    # Set up the hardware WatchDog Timer with an 8-second timeout
    # The max timeout is 8388ms (approx 8.3s)
    wdt = machine.WDT(timeout=8000)
    
    s = True
    while True:
        # Feed the watchdog to let it know the code is still running
        wdt.feed()
        await asyncio.sleep_ms(500)
        blue_led(s)
        s = not s

# --- WiFi Handler ---
async def wifi_han(state):
    wifi_led(not state)
    print('Wifi is ', 'up' if state else 'down')
    await asyncio.sleep(1)

# --- MQTT Connection Handler ---
async def conn_han(client):
    await client.subscribe('personal/ucfnaps/downhamweather/windSpeed_mph', 1)

# --- Main MQTT Loop ---
async def main(client):
    try:
        await client.connect()
    except OSError:
        print('Connection failed.')
        return
    n = 0
    while True:
        await asyncio.sleep(5)
        print('publish', n)
        n += 1

# --- Main Task Manager ---
async def main_manager(client):
    tasks = [
        asyncio.create_task(heartbeat()),
        # asyncio.create_task(timer()),  <-- REMOVED
        asyncio.create_task(flame_effect())
    ]
    try:
        await main(client)
    finally:
        for task in tasks:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

# --- MQTT Configuration ---
config['subs_cb'] = sub_cb
config['wifi_coro'] = wifi_han
config['connect_coro'] = conn_han
config['clean'] = True

# Set up client
MQTTClient.DEBUG = True
client = MQTTClient(config)

# --- Run Everything ---
try:
    asyncio.run(main_manager(client))
finally:
    client.close()
    asyncio.new_event_loop()
