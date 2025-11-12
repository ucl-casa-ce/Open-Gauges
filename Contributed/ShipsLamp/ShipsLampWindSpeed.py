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
target_color = OFF  # The color we are fading TOWARDS
display_color = OFF # This is the color currently on the LEDs
global_wind_speed = 0.0 # Stores the last known wind speed for dynamic effects

# --- NEW: Set initial state to CONNECTING_WIFI ---
# This ensures the pixel_controller gives immediate feedback on boot.
global_system_status = "CONNECTING_WIFI" 

# --- Helper function for smooth fading ---
def lerp(a, b, t):
    """Linearly interpolate from a to b by fraction t"""
    return a + (b - a) * t

# --- Fully Continuous Color Blending Function ---
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
        t = wind_speed / 10.0 
        r = 0
        g = int(255 * t)
        b = 0
        return (r, g, b)

    # --- Segment 2 (10-20 mph): Green (0,255,0) -> Yellow (255,255,0) ---
    if wind_speed < 20:
        t = (wind_speed - 10) / 10.0 
        r = int(255 * t)
        g = 255
        b = 0
        return (r, g, b)

    # --- Segment 3 (20-30 mph): Yellow (255,255,0) -> Orange (255,127,0) ---
    if wind_speed < 30:
        t = (wind_speed - 20) / 10.0
        inv_t = 1.0 - t
        r = 255
        g = int(255 * inv_t + 127 * t) 
        b = 0
        return (r, g, b)

    # --- Segment 4 (30-40 mph): Orange (255,127,0) -> Red (255,0,0) ---
    else: # 30 <= wind_speed < 40
        t = (wind_speed - 30) / 10.0
        inv_t = 1.0 - t
        r = 255
        g = int(127 * inv_t) 
        b = 0
        return (r, g, b)


# --- Asynchronous Pixel Control Tasks ---

async def steady_fade_loop():
    """
    This task runs every ~20ms. It smoothly fades the
    display_color towards the target_color.
    """
    global display_color, target_color
    FADE_RATE = 0.05  # Move 5% closer to the target each step
    UPDATE_SLEEP_MS = 20

    if display_color != target_color:
        r = int(lerp(display_color[0], target_color[0], FADE_RATE))
        g = int(lerp(display_color[1], target_color[1], FADE_RATE))
        b = int(lerp(display_color[2], target_color[2], FADE_RATE))

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
    
    base_color = target_color 
    t_wind = min(global_wind_speed, 40.0) / 40.0
    max_reduction = int(lerp(80, 160, t_wind))
    reduction = random.randint(40, max_reduction)

    scale = max(0, (255 - reduction)) / 255.0
    r = int(base_color[0] * scale)
    g = int(base_color[1] * scale)
    b = int(base_color[2] * scale)
    
    display_color = (r, g, b)
    pixels.fill(display_color)
    pixels.show()
    
    await asyncio.sleep_ms(random.randint(50, 150))

async def pixel_controller():
    """
    This is the main state manager.
    It decides *when* to fade and *when* to flicker.
    It also handles the startup visual feedback.
    """
    global display_color, target_color, global_system_status

    # --- STARTUP DISPLAY LOOP ---
    CONNECTING_COLOR = (0, 0, 100) # Dim Blue
    
    print("Pixel controller started, awaiting connection...")
    while global_system_status != "RUNNING":
        if global_system_status == "CONNECTING_WIFI":
            # --- NEW: Pulse Blue for WiFi ---
            # 1. Fade up to Blue
            for i in range(100):
                # If state changes mid-pulse, break out early
                if global_system_status != "CONNECTING_WIFI": break
                t = i / 100.0
                c = int(CONNECTING_COLOR[2] * t)
                display_color = (0, 0, c)
                pixels.fill(display_color)
                pixels.show()
                await asyncio.sleep_ms(10) # 1s fade up

            # 2. Fade down to OFF
            for i in range(100):
                if global_system_status != "CONNECTING_WIFI": break
                t = 1.0 - (i / 100.0)
                c = int(CONNECTING_COLOR[2] * t)
                display_color = (0, 0, c)
                pixels.fill(display_color)
                pixels.show()
                await asyncio.sleep_ms(10) # 1s fade down
            
            # Short pause at the bottom of the pulse
            await asyncio.sleep_ms(200)

        elif global_system_status == "CONNECTING_MQTT":
            # Solid blue to show WiFi is connected, now doing MQTT
            display_color = CONNECTING_COLOR
            pixels.fill(display_color)
            pixels.show()
            await asyncio.sleep_ms(200) # Sleep to allow other tasks
        
        else:
            # Should not happen, but a fallback
            print(f"Unknown system status: {global_system_status}")
            await asyncio.sleep_ms(100)
            
    print("Pixel controller switching to main flame effect loop.")
    # --- END STARTUP LOOP ---


    # --- MAIN FLAME EFFECT LOOP ---
    while True:
        # --- STEADY/FADE PHASE ---
        steady_time_ms = random.randint(10000, 60000)
        start_time = time.ticks_ms()
        
        while time.ticks_diff(time.ticks_ms(), start_time) < steady_time_ms:
            await steady_fade_loop() 

        # --- FLICKER PHASE ---
        if target_color != OFF:
            flicker_duration_ms = random.randint(5000, 15000)
            start_time = time.ticks_ms()
            
            while time.ticks_diff(time.ticks_ms(), start_time) < flicker_duration_ms:
                await flicker_loop()
                
            display_color = target_color
            pixels.fill(display_color)
            pixels.show()

# --- MQTT Callback ---
def sub_cb(topic, msg, retained):
    """Called when a subscribed topic receives a message."""
    global target_color, global_wind_speed
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    try:
        wind = float(msg)
        print(f"Wind speed: {wind} mph")
        global_wind_speed = wind
        target_color = blend_color(wind)
    except ValueError:
        print("Received non-float message")

# --- Heartbeat Task (WITH WATCHDOG) ---
async def heartbeat():
    """
    Blinks the blue LED and feeds the hardware watchdog.
    If this task freezes, the watchdog will reboot the Pico.
    """
    wdt = machine.WDT(timeout=8000)
    s = True
    while True:
        wdt.feed()
        await asyncio.sleep_ms(500)
        blue_led(s)
        s = not s

# --- WiFi Handler ---
async def wifi_han(state):
    """Controls the onboard WiFi LED and updates the system status."""
    global global_system_status
    wifi_led(not state) 
    
    if state:
        print('Wifi is up.')
        global_system_status = "CONNECTING_MQTT"
    else:
        print('Wifi is down. Re-connecting...')
        global_system_status = "CONNECTING_WIFI"
    await asyncio.sleep(1)

# --- MQTT Connection Handler ---
async def conn_han(client):
    """Runs once the MQTT broker is successfully connected."""
    
    # --- STARTUP PULSE ---
    print("MQTT Connected! Running startup pulse...")
    global display_color, global_system_status
    
    # 1. Fade up to White
    for i in range(100):
        c = int(255 * (i / 100.0))
        display_color = (c, c, c)
        pixels.fill(display_color)
        pixels.show()
        await asyncio.sleep_ms(10)
        
    # 2. Fade down to OFF
    for i in range(100):
        c = int(255 * (1.0 - (i / 100.0)))
        display_color = (c, c, c)
        pixels.fill(display_color)
        pixels.show()
        await asyncio.sleep_ms(10)
    
    display_color = OFF
    pixels.fill(display_color)
    pixels.show()
    # --- END STARTUP PULSE ---
    
    print("Subscribing to topic...")
    await client.subscribe('personal/ucfnaps/downhamweather/windSpeed_mph', 1)
    
    print("System is RUNNING.")
    global_system_status = "RUNNING"

# --- Main MQTT Loop ---
async def main(client):
    """Main MQTT client loop."""
    try:
        await client.connect()
    except OSError:
        print('Connection failed.')
        return
    n = 0
    while True:
        await asyncio.sleep(5)
        print('MQTT main loop running...', n)
        n += 1

# --- Main Task Manager ---
async def main_manager(client):
    """Starts and manages all asynchronous tasks."""
    tasks = [
        asyncio.create_task(heartbeat()),
        asyncio.create_task(pixel_controller())
    ]
    try:
        await main(client) # This task handles MQTT
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
