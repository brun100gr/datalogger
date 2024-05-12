#include "FS.h"
#include "SD_MMC.h"
#include "ESPAsyncWebServer.h"

#include "credentials.h"

//#include "CSS.h"  // Includes headers of the web and de style file

// Definire il pin CS (Chip Select) per la scheda SD
#define SD_CS_PIN 5

// Definire il nome del file
const char* filename = "/example.txt";

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

String webpage, MessageLine;
fileinfo Filenames[200]; // Enough for most purposes!
int numfiles;

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(mySSID, myPW);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

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

  /*********  Server Commands  **********/
  // ##################### HOMEPAGE HANDLER ###########################
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Home Page...");
    Home(); // Build webpage ready for display
    request->send(200, "text/html", webpage);
  });

  // ##################### DIR HANDLER ###############################
  server.on("/dir", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("File Directory...");
    Dir(request); // Build webpage ready for display
    request->send(200, "text/html", webpage);
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

void loop() {
  // Non c'è bisogno di eseguire operazioni continuamente
}

//#############################################################################################
void Home() {
  webpage = HTML_Header();
  webpage += "<img src = 'icon' alt='icon'>";
  webpage += HTML_Footer();
}

//#############################################################################################
String HTML_Header() {
  String page;
  page  = F("<!DOCTYPE html><html>");
  page += F("<head>");
  page += F("<title>MC Server</title>"); // NOTE: 1em = 16px
  page += F("<meta name='viewport' content='user-scalable=yes,initial-scale=1.0,width=device-width'>");
  page += F("<style>");//From here style:
  page += F("body{max-width:65%;margin:0 auto;font-family:arial;font-size:100%;}");
  page += F("ul{list-style-type:none;padding:0;border-radius:0em;overflow:hidden;background-color:#d90707;font-size:1em;}");
  page += F("li{float:left;border-radius:0em;border-right:0em solid #bbb;}");
  page += F("li a{color:white; display: block;border-radius:0.375em;padding:0.44em 0.44em;text-decoration:none;font-size:100%}");
  page += F("li a:hover{background-color:#e86b6b;border-radius:0em;font-size:100%}");
  page += F("h1{color:white;border-radius:0em;font-size:1.5em;padding:0.2em 0.2em;background:#d90707;}");
  page += F("h2{color:blue;font-size:0.8em;}");
  page += F("h3{font-size:0.8em;}");
  page += F("table{font-family:arial,sans-serif;font-size:0.9em;border-collapse:collapse;width:85%;}"); 
  page += F("th,td {border:0.06em solid #dddddd;text-align:left;padding:0.3em;border-bottom:0.06em solid #dddddd;}"); 
  page += F("tr:nth-child(odd) {background-color:#eeeeee;}");
  page += F(".rcorners_n {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:20%;color:white;font-size:75%;}");
  page += F(".rcorners_m {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:50%;color:white;font-size:75%;}");
  page += F(".rcorners_w {border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:70%;color:white;font-size:75%;}");
  page += F(".column{float:left;width:50%;height:45%;}");
  page += F(".row:after{content:'';display:table;clear:both;}");
  page += F("*{box-sizing:border-box;}");
  page += F("a{font-size:75%;}");
  page += F("p{font-size:75%;}");
  page += F("</style></head><body><h1>My Circuits</h1>");
  page += F("<ul>");
  page += F("<li><a href='/'>Files</a></li>"); //Menu bar with commands
  page += F("<li><a href='/upload'>Configuration</a></li>"); 
  page += F("</ul>");
  return page;
}

//#############################################################################################
String HTML_Footer() {
  String page;
  page += "<br><br><footer>";
  page += "<p class='medium'>ESP Asynchronous WebServer Example</p>";
  page += "<p class='ps'><i>Copyright &copy;&nbsp;D L Bird 2021 Version " +  Version + "</i></p>";
  page += "</footer>";
  page += "</body>";
  page += "</html>";
  return page;
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
      webpage += "<td><FORM action='/' method='post'><button type='submit' name='download' value='download_" + Fname + "'>Download</button></td>";
      webpage += "<td><FORM action='/' method='post'><button type='submit' name='download' value='delete_" + Fname + "'>Delete</button></td>";
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
  webpage += HTML_Footer();
  request->send(200, "text/html", webpage);
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
/*
<!DOCTYPE html>
<html>
<head>
<title>Page Title</title>
</head>
<body>
<table class='center'>
	<tr>
    	<th>Type</th>
        <th>File Name</th>
        <th>File Size</th>
        <th class='sp'></th>
        <th>Type</th>
        <th>File Name</th>
        <th>File Size</th>
	</tr>
</table>

<table class='center'>
	<tr>
    	<th>Name/Type</th>
      <th style='width:20%'>Type File/Dir</th>
      <th>File Size</th>
	</tr>

	<tr>
    	<td>"File"</td>
        <td>"Name"</td>
        <td>"1234"</td>
        <td>
        	<FORM action='/' method='post'>
            	<button type='submit' name='download' value='download_xxx'>Download</button>
            </FORM>
        </td>
    </tr>
</table>
</body>
</html>
*/
