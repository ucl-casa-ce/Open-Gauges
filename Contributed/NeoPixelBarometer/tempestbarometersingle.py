import requests
import json
import time
from requests import exceptions
import neopixel
import board

#Set Colours
DRY = (255, 0, 0)
YELLOW = (255, 150, 0)
FAIR = (100, 64, 0)
CHANGE = (0, 255, 165)
CYAN = (0, 255, 255)
STORM = (0, 0, 139)
RAIN = (65, 105, 225)
SNOW = (255, 255, 255)

FALLING = (255, 0, 0)
RISING = (0, 0, 225)
STEADY = (0, 255, 0)

SUNNY = (255, 255, 0)
CLOUDY = (0, 255, 255)
PARTLYCLOUDY = (0, 255, 255)
NIGHTCLEAR = (0, 255, 255)
THUNDERSTORM = (255, 150, 0)
SNOW = (255, 255, 255)
FOGGY = (125, 125, 125)

RED = (255, 0, 0)
GREEN = (0, 255, 0)

OFF = (0, 0, 0)


# Set Variables

apikey = 'api_key=YOURAPIKEY'
station_id = 'station_id=YOURSTATIONID'


trend = "text"
baro = 1050
icon = "text"
text = "text"
run = 0


#Set NeoPixel Details - Pin/Number Pixels/Brightness etc

pixels = neopixel.NeoPixel(board.D18, 144, brightness=0.03, auto_write=False)

print ("Running Start Up")

#Start up Lights

n = 1

t_end = time.time() + 22.32 * 1
while time.time() < t_end:
    n = n + 1
    if n >= 144:
        n = 1
    pixels[n] = (SUNNY)
    pixels[n-1] = (STORM)
    pixels.show()
    time.sleep (0.1)
    
pixels.fill((0, 0, 0))

pixels.show()

print ("Getting Conditions and Forecast")

# Get Data from Weather Flow for Station Location

def getconditions():

    try:
        pixels [0] = GREEN
        pixels.show()
        time.sleep(2)
        print ("Getting Data")
        response = requests.get('https://swd.weatherflow.com/swd/rest/better_forecast?' + apikey + '&' + station_id).text
        pixels [0] = OFF
        pixels.show()
        global run
        run = 1
    except requests.exceptions.RequestException as e: 
        pixels [0] = RED
        pixels.show()
        time.sleep(60)
        pixels [0] = OFF
        pixels.show()
        run = 0
        
    data = json.loads(response)
    
   
    global icon
    icon = data['current_conditions']['icon']
    global baro
    baro = int(data['current_conditions']['sea_level_pressure'])
    global trend
    trend = data['current_conditions']['pressure_trend']

    print(icon)
    print(baro)
    print(trend)

# Set Barometer Pixel

def barometer():

    # Pressure top 1050 minus number of pixels to set top pixel
    pixel = 906
    pixelon = int(baro - pixel)

    # Set Colours According to Barometer Reading

    if baro >= 950 and baro <= 970:
        COLOUR = STORM
    elif baro >= 971 and baro <= 989: 
        COLOUR = RAIN
    elif baro >= 990 and baro <= 1014: 
        COLOUR = CHANGE
    elif baro >= 1015 and baro <= 1024: 
        COLOUR = FAIR
    elif baro >= 1025 and baro <= 1050: 
        COLOUR = DRY  
    else: 
        COLOUR = RED

    pixels[pixelon] = (COLOUR)
    

 # Set Trend Pixel
    
def trendpixel():
    

    if trend == 'steady':
     pixels[13] = (STEADY)
    else:
     pixels[13] = (OFF)

    if trend == 'rising':
     pixels[15] = (RISING)
    else:
     pixels[15] = (OFF) 

    if trend == 'falling':
     pixels[11] = (FALLING)
    else:
     pixels[11] = (OFF) 

# Set Conditions/Icon Pixel

def iconpixel():
    
    if icon == 'clear-day':
     pixels[35] = (SUNNY)
    else:
     pixels[35] = (OFF)

    if icon == 'clear-night':
     pixels[31] = (NIGHTCLEAR)
    else:
     pixels[31] = (OFF) 

    if icon == 'partly-cloudy-day' or icon == 'partly-cloudy-night':
     pixels[33] = (CLOUDY)
    else:
     pixels[33]= (OFF)

    if icon == 'cloudy':
     pixels[31] = (CLOUDY)
    else:
     pixels[31] = (OFF) 

    if icon == 'possibly-rainy-day' or icon =='possibly-rainy-night' or icon =='rainy':
     pixels[30] = (RAIN)
    else:
     pixels[30] = (OFF)  

    if icon == 'possibly-sleet-day' or icon =='possibly-sleet-night' or icon =='possibly-snow-day' or icon =='possibly-snow-night' or icon =='snow':
     pixels[25] = (SNOW)
    else:
     pixels[25] = (OFF)   

    if icon == 'thunderstorm' or icon =='possibly-thunderstorm-night' or icon =='possibly-thunderstorm-day':
     pixels[27] = (THUNDERSTORM)
    else:
     pixels[27] = (OFF)    

    if icon == 'foggy':
     pixels[25] = (FOGGY))
    else:
     pixels[25] = (OFF)     

# Run if data is downloaded

while True:

    try:
        getconditions()
        if run == 1:
            barometer()
            trendpixel()
            iconpixel()
            pixels.show()
            time.sleep(60)
    except: 
        time.sleep(10)
        print ("Error Getting Data - Pausing")