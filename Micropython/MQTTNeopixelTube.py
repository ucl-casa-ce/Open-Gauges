# clean.py Test of asynchronous mqtt client with clean session.
# (C) Copyright Peter Hinch 2017-2019.
# Released under the MIT licence.

# Public brokers https://github.com/mqtt/mqtt.github.io/wiki/public_brokers

# The use of clean_session means that after a connection failure subscriptions
# must be renewed (MQTT spec 3.1.2.4). This is done by the connect handler.
# Note that publications issued during the outage will be missed. If this is
# an issue see unclean.py.

# red LED: ON == WiFi fail
# blue LED heartbeat: demonstrates scheduler is running.


#Speed of the servo movement - 0.05 provides a good smooth speed
from neopixel import Neopixel

from mqtt_as import MQTTClient, config
from config import wifi_led, blue_led  # Local definitions
import uasyncio as asyncio
import machine
from machine import Pin, PWM
from time import sleep

#Set up first reading for Tween



speed = 0.05

winddata = [0,0]



#Set up Neopixels - note GRB should be edited according to the neopixel strips order.
numpix = 55
pixels = Neopixel(numpix, 0, 28, "GRB")
pixels.brightness(10)
mqtttopic = "Your MQTT Topic"

YELLOW = (255, 100, 0)
ORANGE = (255, 50, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
RED = (255, 0, 0)
OFF = (0, 0, 0)
WHITE = (15,15,15)

def lights():

    pixels.fill(WHITE)
    pixels.show()   



# First Sweep - Degree Range to be Edited According to Servo and Gear Ratio
def sweep():
    
    
    n= 0
    
    while n < 50 :
        if n < 10:
            COLOUR = BLUE
        
        if n >= 10:
            COLOUR = GREEN
        
        if n >= 20:
            COLOUR = YELLOW
            
        if n >= 30:
            COLOUR = ORANGE    
        
        
        if n >= 40:
            COLOUR = RED    
       
        pixels.set_pixel(n, (COLOUR))
        pixels.set_pixel((n-1), (COLOUR))
        pixels.show()
        sleep(speed)
        n = n+1
        
    sleep(4)    
    n= 50
    while n >= 0 :
      
       
        pixels.set_pixel(n, (COLOUR))
        pixels.set_pixel((n +1), (OFF))
        pixels.show()
        sleep(speed)
        n = n-1



# Subscription callback
def sub_cb(topic, msg, retained):
    
     
    
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    timer()
    wind = float(msg)
    print(wind)

    sleep(speed)
    winddata.append(wind)
  
    
    

     
    if winddata[0] == winddata[-1]:
                sleep(2) 
        
    else:
        
         
        
        while winddata[0] >  winddata[-1]:
           # winddata[0] = winddata[0]*4.5
            if winddata[0] < 10:
                COLOUR = BLUE
            
            if winddata[0] >= 10:
                COLOUR = GREEN
            
            if winddata[0] >= 20:
                COLOUR = YELLOW
                
            if winddata[0] >= 30:
                 COLOUR = ORANGE
                 
            if winddata[0] >= 40:
                 COLOUR = RED      
           
            pixels.set_pixel(int(winddata[0]), (COLOUR))
            pixels.set_pixel(int(winddata[0])+1, (OFF))
            
  
            print('Pixel', int(winddata[0]))
            pixels.show()
            sleep(speed)
            winddata[0] = (winddata[0])-1
            
            if winddata[0] == winddata[-1]:
           # servo(winddata[0])
                sleep(2)
            
         
    while winddata[0] <  winddata[-1]:
        
            if winddata[0] < 10:
                COLOUR = BLUE
            
            if winddata[0] >= 10:
                COLOUR = GREEN
            
            if winddata[0] >= 20:
                COLOUR = YELLOW
                
            if winddata[0] >= 30:
                 COLOUR = ORANGE
                 
            if winddata[0] >= 40:
                 COLOUR = RED   
           # winddata[0] = winddata[0]*4.5
            pixels.set_pixel(int(winddata[0]), (COLOUR))
            pixels.set_pixel(int(winddata[0])-1, (COLOUR))
            pixels.show()
            print('Pixel', int(winddata[0]))
           
            sleep(speed)
            
            winddata[0] = (winddata[0])+1
            
            if winddata[0] == winddata[-1]:
           
                sleep(2)
        
        
     #   if winddata[0] == winddata[-1]:
           # servo(winddata[0])
      #      sleep(2)
            
    if len(winddata) > 600:
            print ("Trimming List")
       
            del winddata[3]      
            
   
#Read and Show Max Wind
            
    maxwind = int(max(winddata))
    pixels [maxwind] = (RED)
    pixels.show()
         
    print(winddata)
    
   
    
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
   
    sweep()
    await asyncio.sleep(1)

    
async def timer():
  
    t= 1
    
    if t > 60:
        machine.reset()
    if t < 60:
        sleep(1)
        t = t+1
        print("Timer:", t)    


# If you connect with clean_session True, must re-subscribe (MQTT spec 3.1.2.4)
async def conn_han(client):
    await client.subscribe(mqtttopic, 1)

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
        print(n)
        if n == 10:
            machine.reset()

# Define configuration
config['subs_cb'] = sub_cb
config['wifi_coro'] = wifi_han
config['connect_coro'] = conn_han
config['clean'] = True

# Set up client
MQTTClient.DEBUG = True  # Optional
client = MQTTClient(config)

asyncio.create_task(heartbeat())
asyncio.create_task(timer())

try:
    asyncio.run(main(client))

finally:
    client.close()  # Prevent LmacRxBlk:1 errors
    asyncio.new_event_loop()