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
target_color = OFF  # Renamed from current_color
display_color = OFF # This is the color currently on the LEDs
global_wind_speed = 0.0 # NEW: Stores the last known wind speed for dynamic effects

# --- NEW: Helper function for smooth fading ---
def lerp(a, b, t):
    """Linearly interpolate from a to b by fraction t"""
    return a + (b - a) * t

# --- NEW: Fully Continuous Color Blending Function ---
def blend_color(wind_speed):
    """
    Calculates a single, continuous gradient across 4 segments:
    0-10 mph: OFF (0,0,0) -> Green (0,255,0)
    10-20 mph: Green (0,255,0) -> Yellow (255,255,0)
    20-30 mph: Yellow (255,255,0) -> Orange (255,127,0)
    30-40 mph: Orange (255,127,0) -> Red (255,0,0)
    >40 mph: Stays Red
    """
    if wind_speed <= 0:
        return OFF  # (0, 0, 0)
    
    if wind_speed >= 40:
        return (255, 0, 0) # Solid Red

    # --- Segment 1 (0-10 mph): OFF (0,0,0) -> Green (0,255,0) ---
    if wind_speed < 10:
        # 't' is the fraction of how far through this 10mph segment we are
        # e.g., 5mph -> t = 0.5
        t = wind_speed / 10.0 
        r = 0
        g = int(255 * t)  # Green fades in from 0 to 255
        b = 0
        return (r, g, b)

    # --- Segment 2 (10-20 mph): Green (0,255,0) -> Yellow (255,255,0) ---
    if wind_speed < 20:
        # 't' is the fraction from 10 to 20
        # e.g., 15mph -> (15-10)/10 -> t = 0.5
        t = (wind_speed - 10) / 10.0 
        r = int(255 * t)     # Red fades in from 0 to 255
        g = 255              # Green stays full
        b = 0
        return (r, g, b)

    # --- Segment 3 (20-30 mph): Yellow (255,255,0) -> Orange (255,127,0) ---
    if wind_speed < 30:
        t = (wind_speed - 20) / 10.0 # e.g., 25mph -> t = 0.5
        inv_t = 1.0 - t
        r = 255 # Red stays full
        # Green fades from 255 (Yellow) down to 127 (Orange)
        g = int(255 * inv_t + 127 * t) 
        b = 0
        return (r, g, b)

    # --- Segment 4 (30-40 mph): Orange (255,127,0) -> Red (255,0,0) ---
    else: # This catches 30 <= wind_speed < 40
        t = (wind_speed - 30) / 10.0 # e.g., 35mph -> t = 0.5
        inv_t = 1.0 - t
        r = 255 # Red stays full
        # Green fades from 127 (Orange) down to 0 (Red)
        g = int(127 * inv_t) 
        b = 0
        return (r, g, b)


# --- Combined Flame Effect Task (REFACTORED) ---

async def steady_fade_loop():
    """
    This task runs every ~20ms. It smoothly fades the
    display_color towards the target_color.
    """
    global display_color, target_color
    FADE_RATE = 0.05  # Move 5% closer to the target each step
    UPDATE_SLEEP_MS = 20

    if display_color != target_color:
        # Calculate the new interpolated color
        r = int(lerp(display_color[0], target_color[0], FADE_RATE))
        g = int(lerp(display_color[1], target_color[1], FADE_RATE))
        b = int(lerp(display_color[2], target_color[2], FADE_RATE))

        # Check if we're "close enough" and snap to the final color
        if (abs(r - target_color[0]) < 2 and
            abs(g - target_color[1]) < 2 and
            abs(b - target_color[2]) < 2):
            display_color = target_color
        else:
            display_color = (r, g, b)
            
        pixels.fill(display_color)
        pixels.show()
    
    await asyncio.sleep_ms(UPDATE_SLEEP_MS)

async def flicker_loop():
    """
    This task runs to generate one "flicker"
    """
    global display_color, target_color, global_wind_speed
    
    base_color = target_color # Flicker around the *target*
    
    # --- DYNAMIC FLICKER ---
    # Make flicker intensity based on wind speed.
    # We map wind speed (0-40mph) to a max flicker dimming (80-160).
    # Low wind (0mph) = gentle flicker (dimming 40-80)
    # High wind (40mph) = wild flicker (dimming 40-160)
    
    # 1. Normalize wind speed to a 0.0 - 1.0 fraction
    t_wind = min(global_wind_speed, 40.0) / 40.0
    
    # 2. Lerp the max dimming amount based on the wind fraction
    max_reduction = int(lerp(80, 160, t_wind))
    
    # 3. Get the final random dimming amount
    reduction = random.randint(40, max_reduction)
    # --- END DYNAMIC FLICKER ---

    scale = max(0, (255 - reduction)) / 255.0
    r = int(base_color[0] * scale)
    g = int(base_color[1] * scale)
    b = int(base_color[2] * scale)
    
    display_color = (r, g, b) # Store this flicker color
    pixels.fill(display_color)
    pixels.show()
    
    await asyncio.sleep_ms(random.randint(50, 150)) # Flicker sleep

async def pixel_controller():
    """
    This is the main state manager, replacing flame_effect.
    It decides *when* to fade and *when* to flicker.
    """
    global display_color, target_color
    while True:
        # --- STEADY/FADE PHASE ---
        # Run the fade loop for a random steady duration
        steady_time_ms = random.randint(10000, 60000)
        start_time = time.ticks_ms()
        
        while time.ticks_diff(time.ticks_ms(), start_time) < steady_time_ms:
            await steady_fade_loop() # This loop now handles fading

        # --- FLICKER PHASE ---
        if target_color != OFF:
            flicker_duration_ms = random.randint(5000, 15000)
            start_time = time.ticks_ms()
            
            while time.ticks_diff(time.ticks_ms(), start_time) < flicker_duration_ms:
                await flicker_loop()
                
            # After flickering, snap back to the target color
            # to prepare for the next fade/steady phase.
            display_color = target_color
            pixels.fill(display_color)
            pixels.show()

# --- MQTT Callback ---
def sub_cb(topic, msg, retained):
    global target_color, global_wind_speed # Added global_wind_speed
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    try:
        wind = float(msg)
        print(wind)
        global_wind_speed = wind # Store the latest speed for the flicker effect
        target_color = blend_color(wind) # Set the target
    except ValueError:
        print("Received non-float message")

# --- Heartbeat Task (WITH WATCHDOG) ---
async def heartbeat():
    # Set up the hardware WatchDog Timer with an 8-second timeout
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
    
    # --- STARTUP PULSE ---
    # Run a visual "pulse" to show we are connected
    # and ready to receive data.
    print("MQTT Connected! Running startup pulse...")
    global display_color
    
    # 1. Fade up to White
    for i in range(100):
        c = int(255 * (i / 100.0))
        display_color = (c, c, c)
        pixels.fill(display_color)
        pixels.show()
        await asyncio.sleep_ms(10) # 1-second fade up
        
    # 2. Fade down to OFF
    for i in range(100):
        c = int(255 * (1.0 - (i / 100.0)))
        display_color = (c, c, c)
        pixels.fill(display_color)
        pixels.show()
        await asyncio.sleep_ms(10) # 1-second fade down
    
    display_color = OFF
    pixels.fill(display_color)
    pixels.show()
    # --- END STARTUP PULSE ---
    
    print("Subscribing to topic...")
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
        asyncio.create_task(pixel_controller()) # Renamed from flame_effect
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
