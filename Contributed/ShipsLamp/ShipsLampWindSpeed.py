from neopixel import Neopixel
from mqtt_as import MQTTClient, config
from config import wifi_led, blue_led
import uasyncio as asyncio
import machine
import random
import time

# --- Set up NeoPixels ---
numpix = 60
# Set color order to "GRB"
pixels = Neopixel(numpix, 0, 15, "GRB")

OFF = (0, 0, 0)
pixels.brightness(100)  # Set LED brightness to 100

winddata = []
current_color = OFF  # Initial color


# --- NEW "Weather Map" Color Blending Function ---
def blend_color(wind_speed):
    """
    Calculates a "weather map" gradient:
    0 = OFF
    0-10 = Solid Green
    10-20 = Green -> Yellow
    20-30 = Yellow -> Orange
    30-40 = Orange -> Red
    >40 = Solid Red
    """
    if wind_speed <= 0:
        return OFF  # (0, 0, 0)
    
    if wind_speed >= 40:
        return (255, 0, 0) # Solid Red

    # --- Segment 1 (0-10 mph): Solid Green ---
    if wind_speed < 10:
        return (0, 255, 0)

    # --- Segment 2 (10-20 mph): Green (0,255,0) -> Yellow (255,255,0) ---
    if wind_speed < 20:
        t = (wind_speed - 10) / 10.0 # Normalize 10-20 to 0.0-1.0
        r = int(255 * t)     # Red fades in
        g = 255              # Green stays full
        b = 0
        return (r, g, b)

    # --- Segment 3 (20-30 mph): Yellow (255,255,0) -> Orange (255,127,0) ---
    if wind_speed < 30:
        t = (wind_speed - 20) / 10.0 # Normalize 20-30 to 0.0-1.0
        r = 255 # Red stays full
        g = int(255 * (1.0 - t) + 127 * t) # Green fades from 255 to 127
        b = 0
        return (r, g, b)

    # --- Segment 4 (30-40 mph): Orange (255,127,0) -> Red (255,0,0) ---
    # This will catch everything from 30 up to 39.99
    else:
        t = (wind_speed - 30) / 10.0 # Normalize 30-40 to 0.0-1.0
        r = 255 # Red stays full
        g = int(127 * (1.0 - t)) # Green fades from 127 to 0
        b = 0
        return (r, g, b)


# --- Combined Flame Effect Task ---
async def flame_effect():
    """
    This single task now handles both steady and flickering states.
    It's the only task that controls the pixels.
    """
    global current_color
    while True:
        
        # --- STEADY PHASE ---
        # Stay steady for 10-60 seconds
        steady_time_ms = random.randint(10000, 60000)
        start_time = time.ticks_ms()
        
        steady_color = current_color
        pixels.fill(steady_color)
        pixels.show()

        while time.ticks_diff(time.ticks_ms(), start_time) < steady_time_ms:
            if current_color != steady_color:
                # MQTT message arrived! Update the steady light.
                steady_color = current_color
                pixels.fill(steady_color)
                pixels.show()
            await asyncio.sleep_ms(200)

        # --- FLICKER PHASE ---
        # Only flicker if the light is NOT off
        if current_color != OFF:
            # Flicker for 5-15 seconds
            flicker_duration_ms = random.randint(5000, 15000)
            start_time = time.ticks_ms()

            while time.ticks_diff(time.ticks_ms(), start_time) < flicker_duration_ms:
                base_color = current_color
                
                # Reduce brightness by a random amount (40-120).
                reduction = random.randint(40, 120)
                scale = max(0, (255 - reduction)) / 255.0
                
                r = int(base_color[0] * scale)
                g = int(base_color[1] * scale)
                b = int(base_color[2] * scale)

                pixels.fill((r, g, b))
                pixels.show()
                
                await asyncio.sleep_ms(random.randint(50, 150))
        
# --- Timer Task ---
async def timer():
    m = 0
    print('Setting timer for 60 mins')
    while True:
        print('minutes', m)
        await asyncio.sleep(60)
        m = m + 1
        if m == 60:
            machine.reset()

# --- MQTT Callback ---
def sub_cb(topic, msg, retained):
    """
    SIMPLIFIED: This function ONLY updates the global variable.
    """
    global current_color
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    try:
        wind = float(msg)
        print(wind)
        current_color = blend_color(wind)
    except ValueError:
        print("Received non-float message")

# --- Heartbeat Task ---
async def heartbeat():
    s = True
    while True:
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
        asyncio.create_task(timer()),
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