#ifndef PAGE_INDEX_H
#define PAGE_INDEX_H

#include <Arduino.h>

const char MAIN_page[] PROGMEM = R"=====(
  <!DOCTYPE html>
  <html>
    <title>Digital Clcok & Scrolling Text with ESP32 and P10 RGB 32x16</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html {font-family: Helvetica, sans-serif;}
      h1 {font-size: 1.1rem; color:#1976D2;}
  
      label {font-size: 14px;}
  
      .div_Form {
        margin: auto;
        width: 90%;
        border:1px solid #D8D8D8;
        border-radius: 10px;
        background-color: #f2f2f2;
        padding: 9px 9px;
      }
  
      .myButton {
        display: inline-block;
        padding: 3px 25px;
        font-size: 13px;
        cursor: pointer;
        text-align: center;
        text-decoration: none;
        outline: none;
        color: #fff;
        background-color: #1976D2;
        border: none;
        border-radius: 8px;
        box-shadow: 0 2px #999;
      }
      .myButton:hover {background-color: #104d89}
      .myButton:active {background-color: #104d89; box-shadow: 0 1px #666; transform: translateY(2px);}
      .myButton:disabled {background-color: #666; box-shadow: 0 1px #666; transform: translateY(2px);}
      
      .myButtonX {background-color: #ff0000}
      .myButtonX:hover {background-color: #7a0101}
      
      .div_Form_Input {display: table; margin: 0px; padding: 0px; box-sizing: border-box;}
      .div_Input_Text {display: table-cell; width: 100%;}
      .div_Input_Text > input {width:99.5%; margin-left: 0px; padding-left: 2px; box-sizing: border-box;}
      
      table, th, td {
        border: 0px solid black;
        border-collapse: collapse;
        font-size: 14px;
      }
      
      .div_Logs {
        width: 100%;
        height: 150px;
        border: 1px solid #D8D8D8;
        border-radius: 5px;
        background-color: #ffffff;
        overflow-y: scroll;
        font-family: 'Courier New', monospace;
        font-size: 12px;
        padding: 5px;
        box-sizing: border-box;
      }
    </style>
    <body>
      <div style="margin: 10px">
        <h1>Digital Clock & Scrolling Text with ESP32 and P10 RGB 32x16</h1>
        
        <div class="div_Form">
          <div style="margin-bottom: 10px;">
            <label>Key : </label>
            <div class="div_Form_Input">
              <div class="div_Input_Text">
                <input type="text" id="key" maxlength="20" placeholder="Enter key">
              </div>
            </div>
          </div>
          
          <div style="margin-bottom: 10px;">
            <button class="myButton" onclick="getSettings()">Get Settings</button>
          </div>
        </div>
        
        <br>
        
        <div class="div_Form">
          <h3>Set Date & Time</h3>
          <table style="width:100%">
            <tr>
              <td style="width:25%"><label>Year : </label></td>
              <td style="width:75%">
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="d_Year" min="2020" max="2099" value="2024">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Month : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="d_Month" min="1" max="12" value="1">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Day : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="d_Day" min="1" max="31" value="1">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Hour : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="t_Hour" min="0" max="23" value="12">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Minute : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="t_Minute" min="0" max="59" value="0">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Second : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="t_Second" min="0" max="59" value="0">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setTimeDate()">Set Date & Time</button>
          </div>
        </div>
        
        <br>
        
        <div class="div_Form">
          <h3>Display Settings</h3>
          <table style="width:100%">
            <tr>
              <td style="width:25%"><label>Display Mode : </label></td>
              <td style="width:75%">
                <select id="input_Display_Mode" style="width:100%">
                  <option value="1">Mode 1 (Manual Colors)</option>
                  <option value="2">Mode 2 (Auto Color Change)</option>
                </select>
              </td>
            </tr>
            <tr>
              <td><label>Brightness : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="input_Brightness" min="0" max="255" value="125">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Scroll Speed : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="input_Scrolling_Speed" min="10" max="100" value="45">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setDisplayMode()">Set Display Mode</button>
            <button class="myButton" onclick="setBrightness()">Set Brightness</button>
            <button class="myButton" onclick="setScrollingSpeed()">Set Scroll Speed</button>
          </div>
        </div>
        
        <br>
        
        <div class="div_Form">
          <h3>Color Settings (Mode 1 Only)</h3>
          <table style="width:100%">
            <tr>
              <td colspan="2"><label><strong>Clock Color (RGB)</strong></label></td>
            </tr>
            <tr>
              <td style="width:10%"><label>R : </label></td>
              <td style="width:90%">
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Clock_R" min="0" max="255" value="255">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>G : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Clock_G" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>B : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Clock_B" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setColorClock()">Set Clock Color</button>
          </div>
          
          <br>
          
          <table style="width:100%">
            <tr>
              <td colspan="2"><label><strong>Date Color (RGB)</strong></label></td>
            </tr>
            <tr>
              <td style="width:10%"><label>R : </label></td>
              <td style="width:90%">
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Date_R" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>G : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Date_G" min="0" max="255" value="255">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>B : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Date_B" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setColorDate()">Set Date Color</button>
          </div>
          
          <br>
          
          <table style="width:100%">
            <tr>
              <td colspan="2"><label><strong>Text Color (RGB)</strong></label></td>
            </tr>
            <tr>
              <td style="width:10%"><label>R : </label></td>
              <td style="width:90%">
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Text_R" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>G : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Text_G" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>B : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Text_B" min="0" max="255" value="255">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setColorText()">Set Text Color</button>
          </div>
        </div>
        
        <br>
        
        <div class="div_Form">
          <h3>Scrolling Text</h3>
          <table style="width:100%">
            <tr>
              <td><label>Text : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="text" id="input_Scrolling_Text" maxlength="150" placeholder="Enter scrolling text">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setScrollingText()">Set Scrolling Text</button>
          </div>
        </div>
        
        <br>
        
        <div class="div_Form">
          <h3>Countdown Timer</h3>
          <table style="width:100%">
            <tr>
              <td style="width:25%"><label>Active : </label></td>
              <td style="width:75%">
                <input type="checkbox" id="countdown_Active" style="transform: scale(1.2);">
              </td>
            </tr>
            <tr>
              <td><label>Title : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="text" id="countdown_Title" maxlength="50" placeholder="Event title (e.g., NEW YEAR)">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Target Date : </label></td>
              <td>
                <div style="display: flex; gap: 5px;">
                  <input type="number" id="countdown_Day" min="1" max="31" style="width:20%;" placeholder="Day">
                  <input type="number" id="countdown_Month" min="1" max="12" style="width:20%;" placeholder="Month">
                  <input type="number" id="countdown_Year" min="2024" max="2099" style="width:25%;" placeholder="Year">
                </div>
              </td>
            </tr>
            <tr>
              <td><label>Target Time : </label></td>
              <td>
                <div style="display: flex; gap: 5px;">
                  <input type="number" id="countdown_Hour" min="0" max="23" style="width:20%;" placeholder="Hour">
                  <input type="number" id="countdown_Minute" min="0" max="59" style="width:20%;" placeholder="Min">
                  <input type="number" id="countdown_Second" min="0" max="59" style="width:20%;" placeholder="Sec">
                </div>
              </td>
            </tr>
            <tr>
              <td colspan="2"><label>Countdown Color (RGB) : </label></td>
            </tr>
            <tr>
              <td style="width:10%"><label>R : </label></td>
              <td style="width:90%">
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Countdown_R" min="0" max="255" value="255">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>G : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Countdown_G" min="0" max="255" value="165">
                  </div>
                </div>
              </td>
            </tr>
            <tr>
              <td><label>B : </label></td>
              <td>
                <div class="div_Form_Input">
                  <div class="div_Input_Text">
                    <input type="number" id="Color_Countdown_B" min="0" max="255" value="0">
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div style="margin-top: 10px;">
            <button class="myButton" onclick="setCountdown()">Set Countdown</button>
            <button class="myButton" onclick="setColorCountdown()">Set Countdown Color</button>
          </div>
        </div>
        
        <br>
        
        <div class="div_Form">
          <h3>System</h3>
          <button class="myButton myButtonX" onclick="resetSystem()">Reset System</button>
        </div>
      </div>
      
      <script>
        //________________________________________________________________________________ sendCommandToESP32()
        function sendCommandToESP32(sta, msg) {
          var xmlhttp;
          if (window.XMLHttpRequest) {
            // code for IE7+, Firefox, Chrome, Opera, Safari
            xmlhttp = new XMLHttpRequest();
          } else {
            // code for IE6, IE5
            xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
          }
          xmlhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
              if (this.responseText == "+ERR") {
                alert("Error !\\rWrong Key !\\rPlease enter the correct key.");
                return;
              }

              if (this.responseText == "+ERR_DM") {
                alert("Error !\\rThis setting is only for Display Mode : 1. \\rPlease change the Display Mode to apply this setting.");
                return;
              }
              
              if (sta == "get") {
                apply_the_Received_Settings(this.responseText);
              } else {
                alert("Settings applied successfully!");
              }
            }
          }
          xmlhttp.open("GET", "settings?" + msg, true);
          xmlhttp.send();
        }
        //________________________________________________________________________________
        
        function getSettings() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          sendCommandToESP32("get", "key=" + key + "&sta=get");
        }
        
        function setTimeDate() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setTimeDate";
          msg += "&d_Year=" + document.getElementById("d_Year").value;
          msg += "&d_Month=" + document.getElementById("d_Month").value;
          msg += "&d_Day=" + document.getElementById("d_Day").value;
          msg += "&t_Hour=" + document.getElementById("t_Hour").value;
          msg += "&t_Minute=" + document.getElementById("t_Minute").value;
          msg += "&t_Second=" + document.getElementById("t_Second").value;
          sendCommandToESP32("set", msg);
        }
        
        function setDisplayMode() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setDisplayMode";
          msg += "&input_Display_Mode=" + document.getElementById("input_Display_Mode").value;
          sendCommandToESP32("set", msg);
        }
        
        function setBrightness() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setBrightness";
          msg += "&input_Brightness=" + document.getElementById("input_Brightness").value;
          sendCommandToESP32("set", msg);
        }
        
        function setScrollingSpeed() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setScrollingSpeed";
          msg += "&input_Scrolling_Speed=" + document.getElementById("input_Scrolling_Speed").value;
          sendCommandToESP32("set", msg);
        }
        
        function setColorClock() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setColorClock";
          msg += "&Color_Clock_R=" + document.getElementById("Color_Clock_R").value;
          msg += "&Color_Clock_G=" + document.getElementById("Color_Clock_G").value;
          msg += "&Color_Clock_B=" + document.getElementById("Color_Clock_B").value;
          sendCommandToESP32("set", msg);
        }
        
        function setColorDate() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setColorDate";
          msg += "&Color_Date_R=" + document.getElementById("Color_Date_R").value;
          msg += "&Color_Date_G=" + document.getElementById("Color_Date_G").value;
          msg += "&Color_Date_B=" + document.getElementById("Color_Date_B").value;
          sendCommandToESP32("set", msg);
        }
        
        function setColorText() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setColorText";
          msg += "&Color_Text_R=" + document.getElementById("Color_Text_R").value;
          msg += "&Color_Text_G=" + document.getElementById("Color_Text_G").value;
          msg += "&Color_Text_B=" + document.getElementById("Color_Text_B").value;
          sendCommandToESP32("set", msg);
        }
        
        function setScrollingText() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setScrollingText";
          msg += "&input_Scrolling_Text=" + encodeURIComponent(document.getElementById("input_Scrolling_Text").value);
          sendCommandToESP32("set", msg);
        }
        
        function setCountdown() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          
          // Validation des champs
          var day = document.getElementById("countdown_Day").value;
          var month = document.getElementById("countdown_Month").value;
          var year = document.getElementById("countdown_Year").value;
          var hour = document.getElementById("countdown_Hour").value;
          var minute = document.getElementById("countdown_Minute").value;
          var second = document.getElementById("countdown_Second").value;
          var title = document.getElementById("countdown_Title").value;
          
          if (!day || !month || !year || !hour || minute === "" || second === "" || !title) {
            alert("Please fill all countdown fields!");
            return;
          }
          
          var msg = "key=" + key + "&sta=setCountdown";
          msg += "&countdown_Active=" + document.getElementById("countdown_Active").checked;
          msg += "&countdown_Day=" + day;
          msg += "&countdown_Month=" + month;
          msg += "&countdown_Year=" + year;
          msg += "&countdown_Hour=" + hour;
          msg += "&countdown_Minute=" + minute;
          msg += "&countdown_Second=" + second;
          msg += "&countdown_Title=" + encodeURIComponent(title);
          sendCommandToESP32("set", msg);
        }
        
        function setColorCountdown() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          var msg = "key=" + key + "&sta=setColorCountdown";
          msg += "&Color_Countdown_R=" + document.getElementById("Color_Countdown_R").value;
          msg += "&Color_Countdown_G=" + document.getElementById("Color_Countdown_G").value;
          msg += "&Color_Countdown_B=" + document.getElementById("Color_Countdown_B").value;
          sendCommandToESP32("set", msg);
        }
        
        function resetSystem() {
          var key = document.getElementById("key").value;
          if (key == "") {
            alert("Please enter the key!");
            return;
          }
          if (confirm("Are you sure you want to reset the system?")) {
            var msg = "key=" + key + "&sta=resetSystem";
            sendCommandToESP32("set", msg);
          }
        }
        
        function apply_the_Received_Settings(receivedSettings) {
          // Fonction pour appliquer les paramètres reçus
          console.log("Settings received:", receivedSettings);
          // Ici vous pouvez analyser la réponse et remplir les champs du formulaire
        }
      </script>
    </body>
  </html>
)=====";

#endif // PAGE_INDEX_H
