#pragma once

#include <ArduinoOTA.h>

#ifdef ESP8266
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#endif

#ifdef ESP32
#include <WebServer.h>
#endif

#define DEBUG 1
#ifdef DEBUG
#define DEBUG_PRINT(x)			Serial.print (x)
#define DEBUG_PRINTDEC(x,DEC)	Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)		Serial.println (x)
#define DEBUG_PRINTLNDEC(x,DEC)	Serial.println (x, DEC)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x,DEC)
#define DEBUG_PRINTLN(x) 
#define DEBUG_PRINTLNDEC(x,DEC)
#endif

/* Style */
String style =
"<style>"
"@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');"
":root{--bg:#13161c;--surface:#1b2026;--surface2:#242a32;--border:#2d343d;--text:#e8eaed;--text-dim:#8b95a3;--accent:#4fb3ff;}"
"*{box-sizing:border-box;}"
"body{background:var(--bg);font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;font-size:14px;color:var(--text);margin:0;padding:16px;-webkit-font-smoothing:antialiased;}"
"#file-input,input,select,button,.btn{height:40px;border-radius:6px;margin:5px auto;font-size:14px;font-family:inherit;}"
"input,select{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:0 10px;}"
"input:focus,select:focus{outline:none;border-color:var(--accent);}"
"#file-input{padding:0;border:1px dashed var(--border);line-height:44px;text-align:center;display:block;cursor:pointer;background:var(--surface2);color:var(--text-dim);}"
"#bar,#prgbar{background-color:var(--surface2);border-radius:10px}#bar{background-color:var(--accent);width:0%;height:10px}"
"div.card{background:var(--surface);border:1px solid var(--border);max-width:600px;margin:5px auto;padding:18px 20px;border-radius:12px;text-align:left;box-shadow:0 1px 3px rgba(0,0,0,.3);}"
"form{margin:0 auto;max-width:600px;text-align:center;}"
".btn{background:var(--accent);color:#0b1117;border:0;padding:0 16px;cursor:pointer;font-weight:600;}"
".btn:hover{filter:brightness(1.1);}"
"svg[data-lastpass-icon=true]{visibility:none;display: none !important;}"
"div[data-lastpass-icon-root=true]{visibility:none;display: none !important;}"
"div[data-lastpass-icon-root]{visibility:none;display: none !important;}"
"div[data-lastpass-root]{visibility:none;display: none !important;}"
"div[data-lastpass-infield]{visibility:none;display: none !important;}"
"div[data-lastpass-infield='true']{visibility:none;display: none !important;}"
"hr.new5 { border: none; border-top: 1px solid var(--border); }"



"</style>";

/* Login page */
String loginIndex =
"<div class='card'><form name=loginForm>"
"<input name=userid data-lpignore='true' placeholder='User ID' width:100%> "
"<input name=pwd data-lpignore='true' placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value='Firmware Update - Login'>"
"<br>";

String loginIndex2 =
"</form></div>"
"<script>"
"const editor ="
"document.querySelector('grammarly-editor-plugin');"
"editor.config = {"
"suggestionCards: 'off'"
"};"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='1llusion_mb')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>"  ;

/* Server Index Page */
String serverIndex = style +
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<div class='card'>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"</div>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"const editor ="
"document.querySelector('grammarly-editor-plugin');"
"editor.config = {"
"suggestionCards: 'off'"
"};"
"</script>";

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

#ifdef ESP8266
ESP8266HTTPUpdateServer _httpUpdater;
ESP8266WebServer _httpServer(80);
#endif

#ifdef ESP32
WebServer _httpServer(80);
#endif

String _webClientReturnString = "";
bool _updatingFirmware = false;

void handleSendToRoot()
{
	_httpServer.sendHeader("Location", "/", true); //Redirect to our html web page 
	_httpServer.send(302, "text/plane", "");
}

void setupOTA()
{
	ArduinoOTA.onStart([]() {
		_updatingFirmware = true;
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH)
			type = "sketch";
		else // U_SPIFFS
			type = "filesystem";

		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		DEBUG_PRINTLN("Start updating " + type);
		});

	ArduinoOTA.onEnd([]() {
		DEBUG_PRINTLN("\nEnd");
		});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		DEBUG_PRINT("Progress: \r"); DEBUG_PRINT((progress / (total / 100))); DEBUG_PRINTLN("%");
		});
	ArduinoOTA.onError([](ota_error_t error) {
		DEBUG_PRINT("Error[%u]: "); DEBUG_PRINTLN(error);
		if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
		else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
		delay(1000);
		ESP.restart();
		});

	ArduinoOTA.begin();
}

