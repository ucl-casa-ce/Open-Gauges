import plasma
from plasma import plasma_stick
import machine
import time
from mqtt_as import MQTTClient, config
from config import wifi_led, blue_led  # Local definitions
import uasyncio as asyncio
from time import sleep


# Set how many LEDs you have
NUM_LEDS = 60

BRIGHTNESS = 1.0

MQTT_Topic = "YOURMQTT_Topic - ie /windspeed"

# The range of readings that we want to map to colours

MIN = 0
MAX = 40

# pick what bits of the colour wheel to use (from 0-360°)
# https://www.cssscript.com/demo/hsv-hsl-color-wheel-picker-reinvented/

HUE_START = 230  # blue
HUE_END = 359  # red

# WS2812 / NeoPixel™ LEDs  - Note - Order may need editing accoring to your LED - ie GRB, RGB etc
led_strip = plasma.WS2812(NUM_LEDS, 0, 0, plasma_stick.DAT, color_order=plasma.COLOR_ORDER_RGB)

# Start updating the LED strip
led_strip.start()


# Subscribes to MQTT Topic and Sets Lights

def sub_cb(topic, msg, retained):
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    
    mqttdata = float(msg)
    print("Wind Speed =  ", mqttdata)
    

    # calculates a colour
    hue = HUE_START + ((mqttdata - MIN) * (HUE_END - HUE_START) / (MAX - MIN))

    # set the leds
    for i in range(NUM_LEDS):
        led_strip.set_hsv(i, hue / 360, 1.0, BRIGHTNESS)

    time.sleep(0.5)
   
# Demonstrate scheduler is operational.
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

# If you connect with clean_session True, must re-subscribe (MQTT spec 3.1.2.4)
async def conn_han(client):
    await client.subscribe(MQTT_Topic, 1)

async def main(client):
    try:
        await client.connect()
    except OSError:
        print('Connection failed.')
        machine.reset()
        return
    n = 0
    while True:
        await asyncio.sleep(5)
       # print('publish', n)
        # If WiFi is down the following will pause for the duration.
        #await client.publish('result', '{} {}'.format(n, client.REPUB_COUNT), qos = 1)
        n += 1

# Define configuration
config['subs_cb'] = sub_cb
config['wifi_coro'] = wifi_han
config['connect_coro'] = conn_han
config['clean'] = True

# Set up client
MQTTClient.DEBUG = True  # Optional
client = MQTTClient(config)

asyncio.create_task(heartbeat())


try:
    asyncio.run(main(client))
    

finally:
    client.close()  # Prevent LmacRxBlk:1 errors
    asyncio.new_event_loop()