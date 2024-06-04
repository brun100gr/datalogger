#include <FS.h>
#include <SD_MMC.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <vector>
#include <NTPClient.h>
#include <Timezone.h>
//#include <WiFiUdp.h>

//#include "CSS.h"  // Includes headers of the web and de style file

// Definire il pin CS (Chip Select) per la scheda SD
#define SD_CS_PIN 5

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//################  VERSION  ###########################################
String    Version = "0.1";   // Programme version, see change log at end

typedef struct
{
  String filename;
  String ftype;
  String fsize;
} fileinfo;

std::map<String, std::vector<String>> configMap;

String webpage, MessageLine;
fileinfo Filenames[200]; // Enough for most purposes!
int numfiles;

// Initialize WiFi and NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Sync every minute

// Define your local time zone and DST rules
TimeChangeRule dstStart = {"DST", Last, Sun, Mar, 2, 120};  // Last Sunday of March at 2:00 AM UTC+2
TimeChangeRule stdStart = {"STD", First, Sun, Nov, 3, 60};  // First Sunday of November at 3:00 AM UTC+1
Timezone timezone(dstStart, stdStart);

std::vector<String> getKeyValue(const String &key) {
  if (configMap.find(key) != configMap.end()) {
    return configMap[key];
  } else {
    Serial.println("Key \"" + key + "\" not found in configuration.");
    return std::vector<String>(); // Return an empty vector if the key is not found
  }
}

void setup() {
  Serial.begin(115200);
  
  // Inizializzare la scheda SD
  Serial.println("Mounting MicroSD Card");
  if (!SD_MMC.begin("/sdcard", false, false, 40000000, 5)) {
    Serial.println("MicroSD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  Serial.print("Card type: ");
  Serial.println(cardType);

  if (cardType == CARD_NONE) {
    Serial.println("No MicroSD Card found");
    return;
  }

  Serial.print("SD_MMC Card Type: ");
  if(cardType == CARD_MMC){
      Serial.println("MMC");
  } else if(cardType == CARD_SD){
      Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
      Serial.println("SDHC");
  } else {
      Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  Serial.println("Scheda SD inizializzata correttamente");

  readFile(SD_MMC, "/config.txt");
  printConfig();

  WiFi.begin(getKeyValue("mySSID").front(), getKeyValue("myPW").front());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize NTP client
  timeClient.begin();

  // Update the NTP client to get the latest time
  timeClient.update();

  /*********  Server Commands  **********/
  // ##################### HOMEPAGE HANDLER ###########################

  // Route to manage both GET and POST requests
  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request){
    if (request->method() == HTTP_GET) {
      // Invia la pagina HTML
      Serial.println("Home Page...HTTP_GET");
      Dir(request); // Build webpage ready for display
      request->send(200, "text/html", webpage);
    } else if (request->method() == HTTP_POST) {
      Serial.println("Home Page...HTTP_POST");

      // Itera attraverso tutti i parametri POST
      int params = request->params();
      for(int i = 0; i < params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        Serial.printf("POST Parameter: %s = %s\n", p->name().c_str(), p->value().c_str());
      }

      if (request->hasArg("view")) {
        // Get the argument with the full file name
        String fullFileName = request->arg("view");

        // Define the prefix to remove
        String prefix = "view_";
        
        // Remove the prefix by using the substring method
        String fileName = fullFileName.substring(prefix.length());
        Serial.printf("View filename: %s\n", fileName);

        AsyncWebServerResponse *response = request->beginResponse(SD_MMC, "/"+fileName, String(), false);
        request->send(response);
      } else if (request->hasArg("download")) {
        // Get the argument with the full file name
        String fullFileName = request->arg("download");

        // Define the prefix to remove
        String prefix = "download_";
        
        // Remove the prefix by using the substring method
        String fileName = fullFileName.substring(prefix.length());
        Serial.printf("Download filename: %s\n", fileName);

        AsyncWebServerResponse *response = request->beginResponse(SD_MMC, "/"+fileName, String(), true);
        request->send(response);
      } else {
        // Invia la pagina HTML
        Dir(request); // Costruisce la pagina web pronta per essere visualizzata
        request->send(200, "text/html", webpage);
      }
    }
  });

  // Add file upload handling
  server.on("/upload", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      Serial.println("Handling upload...");
      request->send(200, "text/plain", "File Uploaded");
    }, 
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      onFileUpload(request, filename, index, data, len, final);
    }
  );

  // Config page handling
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><h1>Hello, ESP32! Configuration page</h1></body></html>");
  });

  server.begin();  // Start the server

/*
  // Aprire il file in modalità scrittura
  File file = SD_MMC.open(filename, FILE_WRITE);
  
  // Controllare se il file è stato aperto correttamente
  if (!file) {
    Serial.println("Errore nell'apertura del file");
    return;
  }
  
  // Scrivere una stringa nel file
  String dataString = "Questo è un esempio di stringa che verrà scritta nel file.";
  file.println(dataString);
  
  // Chiudere il file
  file.close();
  
  Serial.println("Stringa scritta nel file con successo");
*/
}

//#############################################################################################
// File upload handler
void  onFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {  // Start of a new file upload
    Serial.printf("UploadStart: %s\n", filename.c_str());
    request->_tempFile = SD_MMC.open("/" + filename, FILE_WRITE);
  }
  if (request->_tempFile) {
    request->_tempFile.write(data, len);
  }
  if (final) {  // End of file upload
    if (request->_tempFile) {
      request->_tempFile.close();
    }
    Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
  }
}

//#############################################################################################
// Function to display file content on Serial Monitor
void displayFileContent(const String& fileName) {
  File file = SD_MMC.open(fileName);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("File Content:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
  }
  file.close();
}

void loop() {
    // Update the NTP client to get the latest time
    timeClient.update();
    
    // Get current epoch time
    time_t utc = timeClient.getEpochTime();
  
    // Print the epoch time in seconds
    Serial.print("Epoch Time (seconds): ");
    Serial.println(utc);
    
    // Convert to local time with DST adjustment
    time_t local = timezone.toLocal(utc);
    
    // Format the timestamp
    String timestamp = getFormattedTime(local);
    
    Serial.println(timestamp);

    delay(1000);
}

//#############################################################################################
// Convert epoch time to formatted time string
String getFormattedTime(time_t epochTime) {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", year(epochTime), month(epochTime), day(epochTime), hour(epochTime), minute(epochTime), second(epochTime));
  return String(buffer);
}

//#############################################################################################
void Home() {
  webpage = HTML_Header();
  webpage += "<img src = 'icon' alt='icon'>";
  webpage += HTML_Footer();
}

//#############################################################################################
String HTML_Header() {
  String pageHeader;
  pageHeader  = F("<!DOCTYPE html><html>");
  pageHeader += F("<head>");
  pageHeader += F("<title>MC Server</title>"); // NOTE: 1em = 16px
  pageHeader += F("<meta name='viewport' content='user-scalable=yes,initial-scale=1.0,width=device-width'>");
  pageHeader += F("<style>");//From here style:
  pageHeader += F("body{max-width:65%;margin:0 auto;font-family:arial;font-size:100%;}");
  pageHeader += F("ul{list-style-type:none;padding:0;border-radius:0em;overflow:hidden;background-color:#d90707;font-size:1em;}");
  pageHeader += F("li{float:left;border-radius:0em;border-right:0em solid #bbb;}");
  pageHeader += F("li a{color:white; display: block;border-radius:0.375em;padding:0.44em 0.44em;text-decoration:none;font-size:100%}");
  pageHeader += F("li a:hover{background-color:#e86b6b;border-radius:0em;font-size:100%}");
  pageHeader += F("h1{color:white;border-radius:0em;font-size:1.5em;padding:0.2em 0.2em;background:#d90707;}");
  pageHeader += F("h2{color:blue;font-size:0.8em;}");
  pageHeader += F("h3{font-size:0.8em;}");
  pageHeader += F("table{font-family:arial,sans-serif;font-size:0.9em;border-collapse:collapse;width:85%;}"); 
  pageHeader += F("th,td {border:0.06em solid #dddddd;text-align:left;padding:0.3em;border-bottom:0.06em solid #dddddd;}"); 
  pageHeader += F("tr:nth-child(odd) {background-color:#eeeeee;}");
  pageHeader += F(".rcorners_n {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:20%;color:white;font-size:75%;}");
  pageHeader += F(".rcorners_m {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:50%;color:white;font-size:75%;}");
  pageHeader += F(".rcorners_w {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:70%;color:white;font-size:75%;}");
  pageHeader += F(".column{float:left;width:50%;height:45%;}");
  pageHeader += F(".row:after{content:'';display:table;clear:both;}");
  pageHeader += F("*{box-sizing:border-box;}");
  pageHeader += F("a{font-size:75%;}");
  pageHeader += F("p{font-size:75%;}");
  pageHeader += F("</style></head><body><h1>My Circuits</h1>");
  pageHeader += F("<ul>");
  pageHeader += F("<li><a href='/'>Files</a></li>"); //Menu bar with commands
  pageHeader += F("<li><a href='/config'>Configuration</a></li>"); 
  pageHeader += F("</ul>");
  return pageHeader;
}

//#############################################################################################
String HTML_Footer() {
  String pageFooter;
  pageFooter  = F("<br><br><footer>");
  pageFooter += F("<p class='medium'>ESP Asynchronous WebServer Example</p>");
  pageFooter += "<p class='ps'><i>Copyright &copy;&nbsp;Bruno 2024 Version " +  Version + "</i></p>";
  pageFooter += F("</footer>");
  pageFooter += F("</body>");
  pageFooter += F("</html>");
  return pageFooter;
}

//#############################################################################################
String ConvBinUnits(int bytes, int resolution) {
  if      (bytes < 1024)                 {
    return String(bytes) + " B";
  }
  else if (bytes < 1024 * 1024)          {
    return String((bytes / 1024.0), resolution) + " KB";
  }
  else if (bytes < (1024 * 1024 * 1024)) {
    return String((bytes / 1024.0 / 1024.0), resolution) + " MB";
  }
  else return "";
}

//#############################################################################################
void Dir(AsyncWebServerRequest * request) {
  String Fname;
  int index = 0;
  Directory(SD_MMC, "/"); // Get a list of the current files on the FS
  webpage  = HTML_Header();
  webpage += "<h3>Filing System Content</h3><br>";
  if (numfiles > 0) {
    webpage += "<table class='center'>";
    webpage += "<tr><th>Type</th><th>File Name</th><th>File Size</th><th class='sp'></th></tr>";
    while (index < numfiles) {
      Fname = Filenames[index].filename;
      webpage += "<tr>";
      webpage += "<td style = 'width:5%'>" + Filenames[index].ftype + "</td><td style = 'width:25%'>" + Fname + "</td><td style = 'width:10%'>" + Filenames[index].fsize + "</td>";
      webpage += "<td class='sp'></td>";
      webpage += "<td><form action='/' method='post'><button type='submit' name='download' value='download_" + Fname + "'>Download</button></td></form>";
      webpage += "<td><form action='/' method='post'><button type='submit' name='view' value='view_" + Fname + "'>View</button></td></form>";
      webpage += "<td><form action='/' method='post'><button type='submit' name='delete' value='delete_" + Fname + "'>Delete</button></td></form>";
      
      webpage += "</tr>";
      index++;
    }
    webpage += "</table>";
    webpage += "<p style='background-color:yellow;'><b>" + MessageLine + "</b></p>";
    MessageLine = "";
  }
  else
  {
    webpage += "<h2>No Files Found</h2>";
  }
  
  // Add file upload form
  webpage += "<hr>";
  webpage += "<h3>Upload File</h3>";
  webpage += "<form action='/upload/' method='post' enctype='multipart/form-data'>";
  webpage += "<input type='file' name='upload'>";
  webpage += "<input type='submit' value='Upload'>";
  webpage += "</form>";

  webpage += HTML_Footer();
  Serial.println(webpage);
}

//#############################################################################################
void Directory(fs::FS &fs, const char * dirname) {
  numfiles  = 0; // Reset number of FS files counter
  File root = fs.open(dirname);
  if (root) {
    root.rewindDirectory();
    File file = root.openNextFile();
    while (file) { // Now get all the filenames, file types and sizes
      Filenames[numfiles].filename = (String(file.name()).startsWith("/") ? String(file.name()).substring(1) : file.name());
      Filenames[numfiles].ftype    = (file.isDirectory() ? "Dir" : "File");
      Filenames[numfiles].fsize    = ConvBinUnits(file.size(), 1);
      file = root.openNextFile();
      numfiles++;
    }
    root.close();
  }
}

//#############################################################################################
void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Read from file:");
  String line;
  while (file.available()) {
    line = file.readStringUntil('\n');
    parseConfigLine(line);
  }
  file.close();
}

//#############################################################################################
void parseConfigLine(const String &line) {
  // Ignore lines that start with '#'
  if (line.startsWith("#")) {
    return;
  }

  int commaIndex = line.indexOf(',');
  if (commaIndex == -1) {
    Serial.println("Invalid config line: " + line);
    return;
  }

  String key = line.substring(0, commaIndex);
  std::vector<String> values;

  int startIndex = commaIndex + 1;
  int endIndex;
  while ((endIndex = line.indexOf(',', startIndex)) != -1) {
    values.push_back(line.substring(startIndex, endIndex));
    startIndex = endIndex + 1;
  }
  values.push_back(line.substring(startIndex)); // Add the last value

  configMap[key] = values;
}

void printConfig() {
  for (const auto &entry : configMap) {
    Serial.print("Key: " + entry.first + " Values: ");
    for (const auto &value : entry.second) {
      Serial.print(value + " ");
    }
    Serial.println();
  }
}


