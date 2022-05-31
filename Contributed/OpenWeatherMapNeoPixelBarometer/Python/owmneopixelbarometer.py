import requests
import json
import time
from requests import exceptions
import neopixel
import board

#Set Globals - baro, open weather map code & to check if the connection has run

baro = 1050
code = 800
run = 0
trend = "steady"
#Set list for 3 hours (every 5 min updates to determine trend)

global barolist
barolist = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]

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

#Set Open Weather Map API

def getconditions():


    global run

    try:
        gettingdata()
        print ("Getting Data")

        api_key = "YOURAPIKEY"
        lat = "YOUR LAT"
        lon = "YOUR LONG"
        url = "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&appid=%s&units=metric" % (lat, lon, api_key)
        response = requests.get(url)
        data = json.loads(response.text)
        run = 1
        time.sleep (10)
        
    except requests.exceptions.RequestException as e: 
        pixels [0] = RED
        pixels.show()
        print ("Error on Data ", e)
        run = 0
        time.sleep(120)


    if run == 1:
        
        global baro
        baro = int(data ["current"]["pressure"])
        print(baro)

        global code
        code = int(data ["current"]["weather"][0]["id"])
        print (code)

        global barolist
        barolist.append(baro)
        barolist = barolist[-36:]
        
    else:
        print(run)

        
   

# Set Barometer Pixel

def barometer():

   
# Set Colours According to Barometer Reading
#baro Start from Pixel 43 to 143  - 950Mb to 1050Mb, Each Pixel Editable

    if baro >=  950 and baro <  1060:
      pixels[43] = (STORM)
    else:
       pixels[43] = (OFF)

    if baro >=  951 and baro <  1060:
      pixels[44] = (STORM)
    else:
       pixels[44] = (OFF)

    if baro >=  952 and baro <  1060:
      pixels[45] = (STORM)
    else:
       pixels[45] = (OFF)

    if baro >=  953 and baro <  1060:
      pixels[46] = (STORM)
    else:
       pixels[46] = (OFF)   

    if baro >=  954 and baro <  1060:
      pixels[47] = (STORM)
    else:
       pixels[47] = (OFF)
       
    if baro >=  955 and baro <  1060:
      pixels[48] = (STORM)
    else:
       pixels[48] = (OFF)

    if baro >=  956 and baro <  1060:
      pixels[49] = (STORM)
    else:
       pixels[49] = (OFF)
      
    if baro >=  957 and baro <  1060:
      pixels[50] = (STORM)
    else:
       pixels[50] = (OFF)
      
    if baro >=  958 and baro <  1060:
      pixels[51] = (STORM)
    else:
       pixels[51] = (OFF)

    if baro >=  959 and baro <  1060:
       pixels[52] = (STORM)
    else:
       pixels[52] = (OFF)

    if baro >=  960 and baro <  1060:
       pixels[53] = (STORM)
    else:
       pixels[53] = (OFF)

    if baro >=  961 and baro <  1060:
       pixels[54] = (STORM)
    else:
       pixels[54] = (OFF)

    if baro >=  962 and baro <  1060:
       pixels[55] = (STORM)
    else:
       pixels[55] = (OFF)

    if baro >=  963 and baro <  1060:
       pixels[56] = (STORM)
    else:
       pixels[56] = (OFF)

    if baro >=  964 and baro <  1060:
       pixels[57] = (STORM)
    else:
       pixels[57] = (OFF)

    if baro >=  965 and baro <  1060:
       pixels[58] = (STORM)
    else:
       pixels[58] = (OFF)
       
    if baro >=  966 and baro <  1060:
       pixels[59] = (STORM)
    else:
       pixels[59] = (OFF)

    if baro >=  967 and baro <  1060:
       pixels[60] = (STORM)
    else:
       pixels[60] = (OFF)

    if baro >=  968 and baro <  1060:
       pixels[61] = (STORM)
    else:
       pixels[61] = (OFF)
       
    if baro >=  969 and baro <  1060:
       pixels[62] = (STORM)
    else:
       pixels[62] = (OFF)
       
    if baro >=  970 and baro <  1060:
       pixels[63] = (STORM)
    else:
       pixels[63] = (OFF)

    if baro >=  971 and baro <  1060:
       pixels[64] = (STORM)
    else:
       pixels[64] = (OFF)

    if baro >=  972 and baro <  1060:
       pixels[65] = (STORM)
    else:
       pixels[65] = (OFF)   
    
    if baro >= 973 and baro < 1060:
       pixels[66] = (RAIN)
    else:
       pixels[66] = (OFF)

    if baro >=  974 and baro <  1060:
       pixels[67] = (RAIN)
    else:
       pixels[67] = (OFF)

    if baro >=  975 and baro <  1060:
       pixels[68] = (RAIN)
    else:
       pixels[68] = (OFF)

    if baro >=  976 and baro <  1060:
       pixels[69] = (RAIN)
    else:
       pixels[69] = (OFF)

    if baro >=  977 and baro <  1060:
       pixels[70] = (RAIN)
    else:
       pixels[70] = (OFF)

    if baro >=  978 and baro <  1060:
       pixels[71] = (RAIN)
    else:
       pixels[71] = (OFF)

    if baro >=  979 and baro <  1060:
       pixels[72] = (RAIN)
    else:
       pixels[72] = (OFF)

    if baro >=  980 and baro <  1060:
       pixels[73] = (RAIN)
    else:
       pixels[73] = (OFF)

    if baro >=  981 and baro <  1060:
       pixels[74] = (RAIN)
    else:
       pixels[74] = (OFF)

    if baro >=  982 and baro <  1060:
       pixels[75] = (RAIN)
    else:
       pixels[75] = (OFF)

    if baro >=  983 and baro <  1060:
       pixels[76] = (RAIN)
    else:
       pixels[76] = (OFF)

    if baro >=  984 and baro <  1060:
       pixels[77] = (RAIN)
    else:
       pixels[77] = (OFF)

    if baro >=  985 and baro <  1060:
       pixels[78] = (RAIN)
    else:
       pixels[78] = (OFF)

    if baro >=  986 and baro <  1060:
       pixels[79] = (RAIN)
    else:
       pixels[79] = (OFF)

    if baro >=  987 and baro <  1060:
       pixels[80] = (RAIN)
    else:
       pixels[80] = (OFF)

    if baro >=  988 and baro <  1060:
       pixels[81] = (RAIN)
    else:
       pixels[81] = (OFF)

    if baro >=  989 and baro <  1060:
       pixels[82] = (RAIN)
    else:
       pixels[82] = (OFF)
#
    if baro >=  990 and baro <  1060:
       pixels[83] = (RAIN)
    else:
       pixels[83] = (OFF)

    if baro >=  991 and baro <  1060:
       pixels[84] = (RAIN)
    else:
       pixels[84] = (OFF)

    if baro >=  992 and baro <  1060:
       pixels[85] = (RAIN)
    else:
       pixels[85] = (OFF)

    if baro >=  993 and baro <  1060:
       pixels[86] = (CHANGE)
    else:
       pixels[86] = (OFF)

  
    if baro >=  994 and baro <  1060:
       pixels[87] = (CHANGE)
    else:
       pixels[87] = (OFF)

       
    if baro >=  995 and baro <  1060:
       pixels[88] = (CHANGE)
    else:
       pixels[88] = (OFF)
       
    if baro >=  996 and baro <  1060:
       pixels[89] = (CHANGE)
    else:
       pixels[89] = (OFF)
       
    if baro >=  997 and baro <  1060:
       pixels[90] = (CHANGE)
    else:
       pixels[90] = (OFF)
       
    if baro >=  998 and baro <  1060:
       pixels[91] = (CHANGE)
    else:
       pixels[91] = (OFF)
       
    if baro >=  999 and baro <  1060:
       pixels[92] = (CHANGE)
    else:
       pixels[92] = (OFF)
       
    if baro >=  1000 and baro <  1060:
       pixels[93] = (CHANGE)
    else:
       pixels[93] = (OFF)
       
    if baro >=  1001 and baro <  1060:
       pixels[94] = (CHANGE)
    else:
       pixels[94] = (OFF)
       
    if baro >=  1002 and baro <  1060:
       pixels[95] = (CHANGE)
    else:
       pixels[95] = (OFF)

    if baro >=  1003 and baro <  1060:
       pixels[96] = (CHANGE)
    else:
       pixels[96] = (OFF)
       
    if baro >=  1004 and baro <  1060:
       pixels[97] = (CHANGE)
    else:
       pixels[97] = (OFF)
       
    if baro >=  1005 and baro <  1060:
       pixels[98] = (CHANGE)
    else:
       pixels[98] = (OFF)
       
    if baro >=  1006 and baro <  1060:
       pixels[99] = (CHANGE)
    else:
       pixels[99] = (OFF)
       
    if baro >=  1007 and baro <  1060:
       pixels[100] = (CHANGE)
    else:
       pixels[100] = (OFF)     

    if baro >=  1008 and baro <  1060:
       pixels[101] = (CHANGE)
    else:
       pixels[101] = (OFF)
       
    if baro >=  1009 and baro <  1060:
       pixels[102] = (CHANGE)
    else:
       pixels[102] = (OFF)
       
    if baro >=  1010 and baro <  1060:
       pixels[103] = (CHANGE)
    else:
       pixels[103] = (OFF)
       
    if baro >=  1011 and baro <  1060:
       pixels[104] = (CHANGE)
    else:
       pixels[104] = (OFF)
       
    if baro >=  1012 and baro <  1060:
       pixels[105] = (CHANGE)
    else:
       pixels[105] = (OFF)
       
    if baro >=  1013 and baro <  1060:
       pixels[106] = (FAIR)
    else:
       pixels[106] = (OFF)

        
    if baro >=  1014 and baro <  1060:
       pixels[107] = (FAIR)
    else:
       pixels[107] = (OFF)
        
    if baro >=  1015 and baro <  1060:
       pixels[108] = (FAIR)
    else:
       pixels[108] = (OFF) 
    
    if baro >=  1016 and baro <  1060:
       pixels[109] = (FAIR)
    else:
       pixels[109] = (OFF) 
    
    if baro >=  1017 and baro <  1060:
       pixels[110] = (FAIR)
    else:
       pixels[110] = (OFF) 
    
    if baro >=  1018 and baro <  1060:
       pixels[111] = (FAIR)
    else:
       pixels[111] = (OFF) 
    
    if baro >=  1019 and baro <  1060:
       pixels[112] = (FAIR)
    else:
       pixels[112] = (OFF) 
    
    if baro >=  1020 and baro <  1060:
       pixels[113] = (FAIR)
    else:
       pixels[113] = (OFF) 
    
    if baro >=  1021 and baro <  1060:
       pixels[114] = (FAIR)
    else:
       pixels[114] = (OFF) 
    
    if baro >=  1022 and baro <  1060:
       pixels[115] = (FAIR)
    else:
       pixels[115] = (OFF) 
    
    if baro >=  1023 and baro <  1060:
       pixels[116] = (FAIR)
    else:
       pixels[116] = (OFF) 
    
    if baro >=  1024 and baro <  1060:
       pixels[117] = (FAIR)
    else:
       pixels[117] = (OFF) 
    
    if baro >=  1025 and baro <  1060:
       pixels[118] = (FAIR)
    else:
       pixels[118] = (OFF) 
    
    if baro >=  1026 and baro <  1060:
       pixels[119] = (FAIR)
    else:
       pixels[119] = (OFF) 
    
    if baro >=  1027 and baro <  1060:
       pixels[120] = (FAIR)
    else:
       pixels[120] = (OFF) 

    if baro >=  1028 and baro <  1060:
       pixels[121] = (FAIR)
    else:
       pixels[121] = (OFF) 
       
    if baro >=  1029 and baro <  1060:
       pixels[122] = (FAIR)
    else:
       pixels[122] = (OFF) 
       
    if baro >=  1030 and baro <  1060:
       pixels[123] = (FAIR)
    else:
       pixels[123] = (OFF) 
       
    if baro >=  1031 and baro <  1060:
       pixels[124] = (FAIR)
    else:
       pixels[124] = (OFF) 
       
    if baro >=  1032 and baro <  1060:
        pixels[125] = (FAIR)
    else:
       pixels[125] = (OFF)
       
    if baro >=  1033 and baro <  1060:
       pixels[126] = (DRY)
    else:
       pixels[126] = (OFF)

    if baro >=  1034 and baro <  1060:
       pixels[127] = (DRY)
    else:
       pixels[127] = (OFF)
       
    if baro >=  1035 and baro <  1060:
       pixels[128] = (DRY)
    else:
       pixels[128] = (OFF)

    if baro >=  1036 and baro <  1060:
       pixels[129] = (DRY)
    else:
       pixels[129] = (OFF)

    if baro >=  1037 and baro <  1060:
       pixels[130] = (DRY)
    else:
       pixels[130] = (OFF)

    if baro >=  1038 and baro <  1060:
       pixels[131] = (DRY)
    else:
       pixels[131] = (OFF)

    if baro >=  1039 and baro <  1060:
       pixels[132] = (DRY)
    else:
       pixels[132] = (OFF)

    if baro >=  1040 and baro <  1060:
       pixels[133] = (DRY)
    else:
       pixels[133] = (OFF)

    if baro >=  1041 and baro <  1060:
       pixels[134] = (DRY)
    else:
       pixels[134] = (OFF)

    if baro >=  1042 and baro <  1060:
       pixels[135] = (DRY)
    else:
       pixels[135] = (OFF)

    if baro >=  1043 and baro <  1060:
       pixels[136] = (DRY)
    else:
       pixels[136] = (OFF)

    if baro >=  1044 and baro <  1060:
       pixels[137] = (DRY)
    else:
       pixels[137] = (OFF)

    if baro >=  1045 and baro <  1060:
       pixels[138] = (DRY)
    else:
       pixels[138] = (OFF)

    if baro >=  1046 and baro <  1060:
       pixels[139] = (DRY)
    else:
       pixels[139] = (OFF)

    if baro >=  1047 and baro <  1060:
       pixels[140] = (DRY)
    else:
       pixels[140] = (OFF)

    if baro >=  1048 and baro <  1060:
       pixels[141] = (DRY)
    else:
       pixels[141] = (OFF)

    if baro >=  1049 and baro <  1060:
       pixels[142] = (DRY)
    else:
       pixels[142] = (OFF)

    if baro >=  1050 and baro <  1060:
       pixels[143] = (DRY)
    else:
       pixels[143] = (OFF)
        
        
def current():

    #Code Ranges

    global code
    
    scatteredclouds = [802,803]
    showers = [521,522,531]
    heavyrain = [502, 503, 504]
    thunderstorm = [200, 201, 202, 210, 211, 212, 221, 230, 231, 232]
    snow = [600, 601, 602, 620, 622]
    sleet = [611, 615]
    fog = [701, 741]


    if code == 800: 
        decoded = "Sunny"
        pixels[36] = (SUNNY)
    else:
     pixels[36] = (OFF)
        

    # Cloud State    

    if code == 801:
        decoded = "Few Clouds"
        pixels[34] = (CLOUDY)
    else:
        pixels[34]= (OFF)

    if code in scatteredclouds:
        decoded = "Scattered Clouds"
        pixels[36] = (CLOUDY)
    else:
        pixels[36]= (OFF)
        
    if code == 804:
        decoded = "Overcast"
        conditionsangle = 47

    # Rain

    if code in showers:
         decoded = "Showers"
         conditionsangle = 55

    if code == 500:
         decoded = "Light Rain"
         pixels[30] = (RAIN)
    else:
     pixels[30] = (OFF)  

    if code == 501:
         decoded = "Moderate Rain"
         pixels[30] = (RAIN)
    else:
         pixels[30] = (OFF)  
        

    if code in heavyrain:
        decoded = "Heavy Rain"
        pixels[30] = (RAIN)
    else:
        pixels[30] = (OFF)  
           

    if code in thunderstorm:
         decoded = "Thunderstorms"
         pixels[27] = (THUNDERSTORM)
    else:
         pixels[27] = (OFF) 
       

    if code in snow:
         decoded = "Snow"
         pixels[25] = (SNOW)
    else:
        pixels[25]= (OFF)
         

    if code in sleet:
         decoded = "Sleet"
         pixels[25] = (SNOW)
    else:
        pixels[25]= (OFF)

    if code in fog:
         decoded = "Mist / Fog"
         pixels[25] = (FOGGY)
    else:
         pixels[25] = (OFF)     
         

    print(decoded)


def trend():

    global barolist
    print (barolist)
    

    trend = (barolist[-1]) - (barolist[-30])
    print ("Trend = ", trend)

    if trend <= 0.25 and trend >= -0.25:
     pixels[14] = (STEADY)
    else:
     pixels[14] = (OFF)

    if trend > 0.25:
     pixels[16] = (RISING)
    else:
     pixels[16] = (OFF) 

    if trend < -0.25:
     pixels[12] = (FALLING)
    else:
     pixels[12] = (OFF) 

def trendbasic():

    global barolist
    print (barolist)
    

    trend = (barolist[-1]) - (barolist[-30])
    print ("Trend = ", trend)

    if trend <= 0.25 and trend >= -0.25:
     pixels[18] = (STEADY)
     pixels[19] = (STEADY)
     pixels[20] = (STEADY)
    else:
     pixels[18] = (OFF)
     pixels[19] = (OFF)
     pixels[20] = (OFF)
     

    if trend > 0.25:
     
     pixels[27] = (RISING)
     pixels[28] = (RISING)
     pixels[29] = (RISING)
     pixels[30] = (RISING)
     
    else:
     
     pixels[27] = (OFF)
     pixels[28] = (OFF)
     pixels[29] = (OFF)
     pixels[30] = (OFF)
     

    if trend < -0.25:
     pixels[8] = (FALLING)
     pixels[9] = (FALLING)
     pixels[10] = (FALLING)
    else:
     pixels[8] = (OFF)
     pixels[9] = (OFF)
     pixels[10] = (OFF) 

def gettingdata():

   
    pixels [0] = GREEN
    pixels.show()
    time.sleep(2)
    pixels [0] = (OFF)
    pixels.show()
    time.sleep(2)
    pixels [0] = GREEN
    pixels.show()
    time.sleep(2)
        
        
        
while True:
    try:
        getconditions()
        barometer()
        trendbasic()
        pixels.show()
        print ("Data Successful, Now Sleeping")
        time.sleep (300)
        
       
    except Exception as e:
        print ("Data Error, Now Sleeping, Trying Again in 5 Minutes")
        print(e)
        time.sleep (300)
