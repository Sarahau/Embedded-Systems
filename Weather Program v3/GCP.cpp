#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include "FS.h"                 // SD Card ESP32
#include <EEPROM.h>             // read and write from flash memory
#include <NTPClient.h>          // Time Protocol Libraries
#include <WiFiUdp.h>            // Time Protocol Libraries
#include <Adafruit_VCNL4040.h>  // Sensor libraries
#include "Adafruit_SHT4x.h"     // Sensor libraries

// Firestore URL addresses
const String URL_GCF_UPLOAD = "https://us-west2-egr-425-380621.cloudfunctions.net/upload";
const String URL_GFC_AVERAGE = "https://us-west2-egr-425-380621.cloudfunctions.net/average";

// Variables
// WiFi Credentials
String wifiNetworkName = "CBU-LancerArmsWest"; // TODO
String wifiPassword = "";

// Initialize library objects (sensors and Time protocols)
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelayMs = 5000; 

// Cloud Function Response Variable
static String jsonAverageResponse;

// Device Details Structure
struct deviceDetails {
    int prox;
    int ambientLight;
    int whiteLight;
    double rHum;
    double temp;
    double accX;
    double accY;
    double accZ;
};

// Device variables
static deviceDetails thisDeviceDetails;
static String userId = "raz"; // TODO update

// Sreen variables
enum Screen
{
    S_INTRO,
    S_UPLOAD,
    S_FETCH,
    S_RESULTS,
    S_WAITING
};

static Screen screen;

String userIdOptions[3] = { "raz", "taz", "all" };
int timeDurationOptions[3] = { 5, 30, 120 };
String dataTypeOptions[3] = { "temp", "rHum", "prox" };
String actionOptions[3] = { "restart", "home", "get average" };

static String selectedUserId;
static int selectedTimeDurationOption;
static String selectedDataType;
static String selectedAction;

// LCD variables
int sWidth;
int sHeight;

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders);
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details);
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details);
bool gcfGetWithReqHeader(String serverUrl, String userId, int duration, String dataType);
String generateReqDetailsHeader(String userId, int duration, String dataType);
double convertFintoC(double f);
double convertCintoF(double c);
void drawUploadDisplay(deviceDetails details, String userId);
void drawFetchDisplay();
void drawWaitingResultsDisplay();
void drawResultsDisplay();
void drawSelectionBox(int row, int column);
void drawIntroScreen();

void setup()
{
    // Initialize the device
    M5.begin();
    M5.IMU.Init();
    
    // Set screen orientation and get height/width
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    // set the states
    screen = S_INTRO;
    drawIntroScreen();

    // Initialize VCNL4040
    if (!vcnl4040.begin()) {
        Serial.println("Couldn't find VCNL4040 chip");
        while (1) delay(1);
    }
    Serial.println("Found VCNL4040 chip");

    // Initialize SHT40
    if (!sht4.begin())
    {
        Serial.println("Couldn't find SHT4x");
        while (1) delay(1);
    }
    Serial.println("Found SHT4x sensor");

    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    // initialize request variables
    selectedUserId = "";
    selectedTimeDurationOption = -1;
    selectedDataType = "";
    selectedAction = "";

    // Initialize Cloud Function Variable
    jsonAverageResponse = "";

    // Connect to WiFi
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());
    
    // Init time connection
    timeClient.begin();
    timeClient.setTimeOffset(0);   
}


void loop()
{
    M5.update();

    if (screen == S_INTRO) {
        if (M5.BtnB.wasPressed()) {
            drawUploadDisplay(thisDeviceDetails, userId);
            screen = S_UPLOAD;
        }
    } else if (screen == S_UPLOAD) {
        if (M5.BtnC.wasPressed()) {
            drawFetchDisplay();
            screen = S_FETCH;
        }
    } else if (screen == S_FETCH) {
        // listen for user choices
        if (selectedUserId == "") {
            if (M5.BtnA.wasPressed()) {
                selectedUserId = userIdOptions[0];
                drawSelectionBox(1,1);
            } else if (M5.BtnB.wasPressed()) {
                selectedUserId = userIdOptions[1];
                drawSelectionBox(1,2);
            } else if (M5.BtnC.wasPressed()) {
                selectedUserId = userIdOptions[2];
                drawSelectionBox(1,3);
            }
        } else if (selectedUserId != "" && selectedTimeDurationOption == -1) {
            if (M5.BtnA.wasPressed()) {
                selectedTimeDurationOption = timeDurationOptions[0];
                drawSelectionBox(2, 1);
            } else if (M5.BtnB.wasPressed()) {
                selectedTimeDurationOption = timeDurationOptions[1];
                drawSelectionBox(2, 2);
            } else if (M5.BtnC.wasPressed()) {
                selectedTimeDurationOption = timeDurationOptions[2];
                drawSelectionBox(2, 3);
            }
        } else if (selectedUserId != "" && selectedTimeDurationOption != -1 && selectedDataType == "") {
            if (M5.BtnA.wasPressed()) {
                selectedDataType = dataTypeOptions[0];
                drawSelectionBox(3, 1);
            } else if (M5.BtnB.wasPressed()) {
                selectedDataType = dataTypeOptions[1];
                drawSelectionBox(3, 2);
            } else if (M5.BtnC.wasPressed()) {
                selectedDataType = dataTypeOptions[2];
                drawSelectionBox(3, 3);
            }
        } else if (selectedUserId != "" && selectedTimeDurationOption != -1 && selectedDataType != "") {
            if (M5.BtnA.wasPressed()) {
                // restart the chosing process from scratch
                selectedUserId = "";
                selectedTimeDurationOption = -1;
                selectedDataType = "";

                M5.Lcd.clear(TFT_BLACK);
                drawFetchDisplay();
            } else if (M5.BtnB.wasPressed()) {
                // go back to the upload screen
                selectedUserId = "";
                selectedTimeDurationOption = -1;
                selectedDataType = "";

                M5.Lcd.clear(TFT_BLACK);
                drawUploadDisplay(thisDeviceDetails, userId);
                screen = S_UPLOAD;
            } else if (M5.BtnC.wasPressed()) {
                // go to waiting screen so that user knows the button pressing was successful :)
                M5.Lcd.clear(TFT_BLACK);
                drawWaitingResultsDisplay();
                screen = S_WAITING;
                
                // get the average data from Firestore w selected values
                jsonAverageResponse = ""; // clear the old average response data
                gcfGetWithReqHeader(URL_GFC_AVERAGE, selectedUserId, selectedTimeDurationOption, selectedDataType); 
                selectedUserId = "";
                selectedTimeDurationOption = -1;
                selectedDataType = ""; 
            }
        }
    } else if (screen == S_WAITING) {
        if (jsonAverageResponse != "") {
            // there is now data in the response JSON
            drawResultsDisplay();
            screen = S_RESULTS;
        }
    } else if (screen == S_RESULTS) {
        if (M5.BtnA.wasPressed()) { 
            drawUploadDisplay(thisDeviceDetails, userId);
            screen = S_UPLOAD;
        }
    }   

    // read and send the device's measured values to Firestore every 5 seconds if connected to wifi
    if (((millis() - lastTime) > timerDelayMs)) {
        // refresh the screen
        if (screen == S_UPLOAD) {
            drawUploadDisplay(thisDeviceDetails, userId);
        }
        // Delay: Update the last time to NOW
        lastTime = millis();

        // Read Sensor Values
        // Read VCNL4040 Sensors
        uint16_t prox = vcnl4040.getProximity();
        uint16_t ambientLight = vcnl4040.getLux();
        uint16_t whiteLight = vcnl4040.getWhiteLight();

        // Read SHT40 Sensors
        sensors_event_t rHum, temp;
        sht4.getEvent(&rHum, &temp); // populate temp and humidity objects with fresh data

        // Read M5's Internal Accelerometer (MPU 6886)
        float accX;
        float accY;
        float accZ;
        M5.IMU.getAccelData(&accX, &accY, &accZ);
        accX *= 9.8;
        accY *= 9.8;
        accZ *= 9.8;
        
        // Get current time as timestamp of last update
        timeClient.update();
        unsigned long epochTime = timeClient.getEpochTime();
        unsigned long long epochMillis = ((unsigned long long)epochTime)*1000;
        struct tm *ptm = gmtime ((time_t *)&epochTime);
        
        // Device details
        thisDeviceDetails.prox = prox;
        thisDeviceDetails.ambientLight = ambientLight;
        thisDeviceDetails.whiteLight = whiteLight;
        thisDeviceDetails.temp = temp.temperature;
        thisDeviceDetails.rHum = rHum.relative_humidity;
        thisDeviceDetails.accX = accX;
        thisDeviceDetails.accY = accY;
        thisDeviceDetails.accZ = accZ;

        // Post data to Firestore
        gcfGetWithHeader(URL_GCF_UPLOAD, userId, epochTime, &thisDeviceDetails);
    }
}

////////////////////////////////////////////////////////////////////
// This method takes in a user ID, time and structure describing
// device details and makes a GET request with the data. 
////////////////////////////////////////////////////////////////////
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details) {
    // Allocate arrays for headers
	const int numHeaders = 1;
    String headerKeys [numHeaders] = {"M5-Details"};
    String headerVals [numHeaders];

    // Add formatted JSON string to header
    headerVals[0] = generateM5DetailsHeader(userId, time, details);
    
    // Attempt to post the file
    Serial.println("Attempting post data.");
    int resCode = httpGetWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}

////////////////////////////////////////////////////////////////////
// Generates the JSON header with sensor details and user data and serializes to a String.
////////////////////////////////////////////////////////////////////
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details) {
    // Allocate M5-Details Header JSON object
    StaticJsonDocument<650> objHeaderM5Details; //DynamicJsonDocument  objHeaderGD(600);
    
    // Add VCNL details
    JsonObject objVcnlDetails = objHeaderM5Details.createNestedObject("vcnlDetails");
    objVcnlDetails["prox"] = details->prox;
    objVcnlDetails["al"] = details->ambientLight;
    objVcnlDetails["rwl"] = details->whiteLight;

    // Add SHT details
    JsonObject objShtDetails = objHeaderM5Details.createNestedObject("shtDetails");
    objShtDetails["temp"] = details->temp;
    objShtDetails["rHum"] = details->rHum;

    // Add M5 Sensor details
    JsonObject objM5Details = objHeaderM5Details.createNestedObject("m5Details");
    objM5Details["ax"] = details->accX;
    objM5Details["ay"] = details->accY;
    objM5Details["az"] = details->accZ;

    // Add Other details
    JsonObject objOtherDetails = objHeaderM5Details.createNestedObject("otherDetails");
    objOtherDetails["timeCaptured"] = time;
    objOtherDetails["userId"] = userId;

    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderM5Details) + 1;
    char cHeaderM5Details [jsonSize];
    serializeJson(objHeaderM5Details, cHeaderM5Details, jsonSize);
    String strHeaderM5Details = cHeaderM5Details;
    //Serial.println(strHeaderM5Details.c_str()); // Debug print

    // Return the header as a String
    return strHeaderM5Details;
}

////////////////////////////////////////////////////////////////////
// This method makes a GET request with the averaging query request header parameters
////////////////////////////////////////////////////////////////////
bool gcfGetWithReqHeader(String serverUrl, String userId, int duration, String dataType) {
    // Allocate arrays for headers
	const int numHeaders = 1;
    String headerKeys [numHeaders] = {"Req-Details"};
    String headerVals [numHeaders];

    // Add formatted JSON string to header
    headerVals[0] = generateReqDetailsHeader(userId, duration, dataType);
    
    // Attempt to post the file
    Serial.println("Attempting to get average data.");
    int resCode = httpGetWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}

////////////////////////////////////////////////////////////////////
// Generates the JSON header with request details and serializes to a String.
////////////////////////////////////////////////////////////////////
String generateReqDetailsHeader(String userId, int duration, String dataType) {
    // Allocate Req-Details Header JSON object
    StaticJsonDocument<650> objHeaderReqDetails; //DynamicJsonDocument  objHeaderGD(600);

    objHeaderReqDetails["userId"] = userId;
    objHeaderReqDetails["timeDuration"] = duration;
    objHeaderReqDetails["dataType"] = dataType;

    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderReqDetails) + 1;
    char cHeaderReqDetails [jsonSize];
    serializeJson(objHeaderReqDetails, cHeaderReqDetails, jsonSize);
    String strHeaderReqDetails = cHeaderReqDetails;
    // Serial.println(strHeaderReqDetails.c_str()); // Debug print

    // Return the header as a String
    return strHeaderReqDetails;
}

////////////////////////////////////////////////////////////////////
// This method takes in a serverURL and array of headers and makes
// a GET request with the headers attached and then returns the response.
////////////////////////////////////////////////////////////////////
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());
    
	////////////////////////////////////////////////////////////////////
	// Add all the headers supplied via parameter
	////////////////////////////////////////////////////////////////////
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Post the headers (NO FILE)
    int httpResCode = http.GET();
    String httpResString = http.getString();

    // Print the response code and message
    Serial.printf("HTTP%scode: %d\n%s\n\n", httpResCode > 0 ? " " : " ERROR ", httpResCode, httpResString.c_str());

    if (serverURL == URL_GFC_AVERAGE) {
        jsonAverageResponse = httpResString;
    }

    // Free resources and return response code
    http.end();
    return httpResCode;
}

/////////////////////////////////////////////////////////////////
// Convert between F and C temperatures
/////////////////////////////////////////////////////////////////
double convertFintoC(double f) { return (f - 32) * 5.0 / 9.0; }
double convertCintoF(double c) { return (c * 9.0 / 5.0) + 32; }

void drawUploadDisplay(deviceDetails details, String userId){    
    int pad = 10;

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.printf("Local Readings from: %s", userId);

    //temp
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.print("Temp: ");
    M5.Lcd.setTextColor(TFT_PINK);
    M5.Lcd.printf("%0.f%", details.temp);

    M5.Lcd.print("  ");

    //Humidity
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Hum: ");
    M5.Lcd.setTextColor(TFT_PINK);
    M5.Lcd.printf("%0.f%%", details.rHum);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);

    //Prox
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Prox:");
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.printf("%i ", details.prox);

    //White light
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("RWL:");
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.printf("%i", details.whiteLight);

    M5.Lcd.print(" ");

    //Ambient light
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("AL:");
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.printf("%i", details.ambientLight);

    M5.Lcd.print(" ");

    //AccX
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Acc X:");
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.printf("%0.3f ", details.accX);

    //AccY
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Acc Y:");
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.printf("%0.3f", details.accY);


    //AccZ
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Acc Z:");
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.printf("%0.3f ", details.accZ);
    M5.Lcd.print(" ");

    // timestamp
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 44);
    M5.Lcd.setTextColor(TFT_WHITE);

    unsigned long epochTime = timeClient.getEpochTime();
    epochTime -= 3600 * 7;
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    String currTime = (String)(ptm->tm_mon) + "/" + (String)(ptm->tm_mday) + "/" + (String)(ptm->tm_year+1900) + ", " + (String)(timeClient.getHours() % 12) + ":" + (String)(timeClient.getMinutes()) + ":" + (String)(timeClient.getSeconds()) + " " + (timeClient.getHours() < 12 ? "AM" : "PM");
    
    M5.Lcd.print(currTime);

    //buttons
     M5.Lcd.setTextColor(TFT_DARKCYAN);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(pad+10+90+100, 40+10+70+70+40);
        M5.Lcd.print("fetch data");
}
void drawFetchDisplay(){
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    int pad = 20;
    int yOffset = 2*pad;
    M5.Lcd.setTextSize(2);
    // Buttons for userId ("raz", “taz”, “All”)
        M5.Lcd.setCursor(pad, pad);
        M5.Lcd.print("User ID:");
        
        M5.Lcd.setCursor(pad+10, yOffset+10);
        M5.Lcd.print("raz");
        M5.Lcd.setCursor(pad+10+90, yOffset+10);
        M5.Lcd.print("taz");
        M5.Lcd.setCursor(pad+10+90+90, yOffset+10);
        M5.Lcd.print("all");
        
    // Buttons for timeDuration (“5s”, “30s”, “120s”)
        M5.Lcd.setCursor(pad, yOffset+50);
        M5.Lcd.print("Time Dur:");
        
        M5.Lcd.setCursor(pad+10, yOffset+10+70);
        M5.Lcd.print("5s");
        M5.Lcd.setCursor(pad+10+90, yOffset+10+70);
        M5.Lcd.print("30s");
        M5.Lcd.setCursor(pad+10+90+90, yOffset+10+70);
        M5.Lcd.print("120s");
    // Buttons for dataType (“Temp”, “rHum”, “prox”)
        M5.Lcd.setCursor(pad, yOffset+50+20+50);
        M5.Lcd.print("Data Type:");

        M5.Lcd.setCursor(pad+10, yOffset+10+70+70);
        M5.Lcd.print("temp");
        M5.Lcd.setCursor(pad+10+90, yOffset+10+70+70);
        M5.Lcd.print("rHum");
        M5.Lcd.setCursor(pad+10+90+90, yOffset+10+70+70);
        M5.Lcd.print("prox");
    // iv) A button to “Get Average”
        M5.Lcd.setTextColor(TFT_DARKCYAN);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(pad+10, yOffset+10+70+70+40);
        M5.Lcd.print("restart");
        M5.Lcd.setCursor(pad+20+100, yOffset+10+70+70+40);
        M5.Lcd.print("home");
        M5.Lcd.setCursor(pad+10+90+100, yOffset+10+70+70+40);
        M5.Lcd.print("get average");
    
}
void drawSelectionBox(int row, int column){
    int pad = 20;
    int yOffset = 40;
    switch(row){
        case 1:
            if(column == 1)
                M5.Lcd.drawRect(pad, yOffset, 80, 40, TFT_MAGENTA);
            else if(column == 2)
                M5.Lcd.drawRect(pad+90, yOffset, 80, 40, TFT_MAGENTA);
            else if(column == 3)
                M5.Lcd.drawRect(pad+180, yOffset, 80, 40, TFT_MAGENTA);
            break;
        case 2:
            if(column == 1)
                M5.Lcd.drawRect(pad, yOffset+50+20, 80, 40, TFT_MAGENTA);
            else if(column == 2)
                M5.Lcd.drawRect(pad+90, yOffset+50+20, 80, 40, TFT_MAGENTA);
            else if(column == 3)
                M5.Lcd.drawRect(pad+180, yOffset+50+20, 80, 40, TFT_MAGENTA);
            break;
        case 3:
            if(column == 1)
                M5.Lcd.drawRect(pad, yOffset+50+20+50+20, 80, 40, TFT_MAGENTA);
            else if(column == 2)
                M5.Lcd.drawRect(pad+90, yOffset+50+20+50+20, 80, 40, TFT_MAGENTA);
            else if(column == 3)
                M5.Lcd.drawRect(pad+180, yOffset+50+20+50+20, 80, 40, TFT_MAGENTA);
            break;
        default:
            Serial.print("breaky oops");
            break;
    }
}

void drawWaitingResultsDisplay() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, (sHeight / 2) - 10);
    M5.Lcd.print("Waiting for results");
}

void drawResultsDisplay(){
     
    String dataType;
    float averageData;
    float minData;
    float maxData;
    String timeRange;
    int numDataPoints;
    float rateDataCollected;    
        
    StaticJsonDocument<1024> objAverageResDetails; //DynamicJsonDocument
    DeserializationError error = deserializeJson(objAverageResDetails, jsonAverageResponse);
    if (!error) {
        String dT = objAverageResDetails["dataType"];
        float aD = objAverageResDetails["averageData"];
        float minD = objAverageResDetails["minData"];
        float maxD = objAverageResDetails["maxData"];
        String tR = objAverageResDetails["timeRange"];
        int nDP = objAverageResDetails["numDataPoints"];
        float rDC = objAverageResDetails["rateDataCollect"];

        dataType = dT;
        averageData = aD;
        minData = minD;
        maxData = maxD;
        timeRange = tR;
        numDataPoints = nDP;
        rateDataCollected = rDC;
    } else {
        screen = S_WAITING;
        return;
    }
    
    int pad = 10;

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.print("Average ");
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print(dataType);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print(" Readings");

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.print("Average value: ");
    M5.Lcd.setTextColor(TFT_PINK);
    M5.Lcd.printf("%0.2f%", averageData);

    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.print("Number of Data Points: ");
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.printf("%d", numDataPoints);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Data Col. Rate: ");
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.printf("%0.3f", rateDataCollected);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 40);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Min: ");
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.printf("%0.2f", minData);

    M5.Lcd.print(" ");

    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Max:");
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.printf("%0.2f", maxData);

    M5.Lcd.setCursor(pad + 10, M5.Lcd.getCursorY() + 44);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print(timeRange);
    Serial.println("timerange: " + timeRange);

    //button
    M5.Lcd.setTextColor(TFT_DARKCYAN);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(pad+10, 40+10+70+70+40);
    M5.Lcd.print("upload data");
}
void drawIntroScreen(){
    
    M5.Lcd.setCursor(sWidth/5, sHeight/3);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.println("Let's Fetch!");
    
    M5.Lcd.setTextColor(TFT_DARKCYAN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(20+10+100, 40+10+70+70+20);
    M5.Lcd.print("start!");
}
