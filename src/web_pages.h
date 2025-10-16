#ifndef WEB_PAGES_H
#define WEB_PAGES_H

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Coffee Station Control</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        .header { text-align: center; color: #8B4513; margin-bottom: 30px; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .status { background-color: #e8f5e8; }
        .config { background-color: #f8f8ff; }
        input[type="number"] { width: 80px; padding: 5px; margin: 5px; }
        button { background-color: #8B4513; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { background-color: #A0522D; }
        .status-display { font-size: 18px; font-weight: bold; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1 class="header">☕ Coffee Station Control</h1>
        
        <div class="section status">
            <h2>Current Status</h2>
            <div id="status" class="status-display">Loading...</div>
            <div style="margin-top: 15px;">
                <button onclick="toggleHeating()">Toggle Heating</button>
                <button onclick="setBrewMode()">Brew Mode</button>
                <button onclick="setSteamMode()">Steam Mode</button>
            </div>
        </div>
        
        <div class="section config">
            <h2>Temperature Settings</h2>
            <div class="grid">
                <div>
                    <label>Brew Temperature (&deg;C):</label><br>
                    <input type="number" id="brewTemp" step="0.5" min="80" max="100">
                </div>
                <div>
                    <label>Steam Temperature (&deg;C):</label><br>
                    <input type="number" id="steamTemp" step="0.5" min="100" max="170">
                </div>
            </div>
        </div>
        
        <div class="section config">
            <h2>Shot Sizes (seconds)</h2>
            <div class="grid">
                <div><label>Small:</label><br><input type="number" id="shot0" step="0.5" min="5" max="60"></div>
                <div><label>Medium:</label><br><input type="number" id="shot1" step="0.5" min="5" max="60"></div>
                <div><label>Large:</label><br><input type="number" id="shot2" step="0.5" min="5" max="60"></div>
                <div><label>Extra Large:</label><br><input type="number" id="shot3" step="0.5" min="5" max="60"></div>
            </div>
        </div>
        
        <div class="section config">
            <h2>Grind Times (seconds)</h2>
            <div class="grid">
                <div>
                    <label>Single Shot:</label><br>
                    <input type="number" id="grind0" step="0.5" min="5" max="30">
                </div>
                <div>
                    <label>Double Shot:</label><br>
                    <input type="number" id="grind1" step="0.5" min="5" max="30">
                </div>
            </div>
        </div>
        
        <div class="section config">
            <h2>PID Control Parameters</h2>
            <div style="margin-bottom: 15px;">
                <label><b>Control Mode:</b></label><br>
                <input type="checkbox" id="usePID"> Use PID Control (unchecked = simple on/off control)
            </div>
            <div class="grid">
                <div>
                    <label>Proportional (Kp):</label><br>
                    <input type="number" id="pidKp" step="0.1" min="0" max="20">
                </div>
                <div>
                    <label>Integral (Ki):</label><br>
                    <input type="number" id="pidKi" step="0.1" min="0" max="20">
                </div>
                <div>
                    <label>Derivative (Kd):</label><br>
                    <input type="number" id="pidKd" step="0.1" min="0" max="10">
                </div>
            </div>
            <div style="margin-top: 15px;">
                <button onclick="startAutotune()" id="autotuneBtn">Start PID AutoTune</button>
                <button onclick="stopAutotune()" id="stopAutotuneBtn" style="display:none; background-color:#c00;">Stop AutoTune</button>
                <span id="autotuneStatus" style="margin-left: 10px; font-weight: bold;"></span>
            </div>
        </div>
        
        <div class="section config">
            <h2>System Settings</h2>
            <label><input type="checkbox" id="enableInflux"> Enable InfluxDB Logging</label><br>
            <label>Temperature Update Interval (ms):</label>
            <input type="number" id="tempInterval" step="100" min="500" max="5000">
        </div>
        
        <div style="text-align: center; margin-top: 20px;">
            <button onclick="loadConfig()">Reload Config</button>
            <button onclick="saveConfig()">Save Configuration</button>
        </div>
    </div>
    
    <script>
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML = `
                        Temperature: ${data.currentTemp}&deg;C (Target: ${data.targetTemp}&deg;C)<br>
                        Operation: ${data.currentOperation}<br>
                        Heating: ${data.heatingElement ? 'ON' : 'OFF'} | 
                        Pump: ${data.pump ? 'ON' : 'OFF'} | 
                        Grinder: ${data.grinder ? 'ON' : 'OFF'}
                    `;
                });
        }
        
        function loadConfig() {
            fetch('/api/config')
                .then(response => response.json())
                .then(config => {
                    document.getElementById('brewTemp').value = config.brewTemp;
                    document.getElementById('steamTemp').value = config.steamTemp;
                    for(let i = 0; i < 4; i++) {
                        document.getElementById('shot' + i).value = config.shotSizes[i];
                    }
                    for(let i = 0; i < 2; i++) {
                        document.getElementById('grind' + i).value = config.grindTimes[i];
                    }
                    document.getElementById('pidKp').value = config.pidKp;
                    document.getElementById('pidKi').value = config.pidKi;
                    document.getElementById('pidKd').value = config.pidKd;
                    document.getElementById('usePID').checked = config.usePID;
                    document.getElementById('enableInflux').checked = config.enableInfluxDB;
                    document.getElementById('tempInterval').value = config.tempUpdateInterval;
                });
        }
        
        function saveConfig() {
            const config = {
                brewTemp: parseFloat(document.getElementById('brewTemp').value),
                steamTemp: parseFloat(document.getElementById('steamTemp').value),
                shotSizes: [],
                grindTimes: [],
                pidKp: parseFloat(document.getElementById('pidKp').value),
                pidKi: parseFloat(document.getElementById('pidKi').value),
                pidKd: parseFloat(document.getElementById('pidKd').value),
                usePID: document.getElementById('usePID').checked,
                enableInfluxDB: document.getElementById('enableInflux').checked,
                tempUpdateInterval: parseInt(document.getElementById('tempInterval').value)
            };
            
            for(let i = 0; i < 4; i++) {
                config.shotSizes[i] = parseFloat(document.getElementById('shot' + i).value);
            }
            for(let i = 0; i < 2; i++) {
                config.grindTimes[i] = parseFloat(document.getElementById('grind' + i).value);
            }
            
            fetch('/api/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(config)
            })
            .then(response => response.text())
            .then(data => alert(data));
        }
        
        function toggleHeating() {
            fetch('/api/heating/toggle', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                console.log(data);
                updateStatus(); // Refresh status immediately
            });
        }
        
        function setBrewMode() {
            fetch('/api/mode/brew', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                console.log(data);
                updateStatus();
            });
        }
        
        function setSteamMode() {
            fetch('/api/mode/steam', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                console.log(data);
                updateStatus();
            });
        }
        
        function startAutotune() {
            if (confirm('AutoTune will take several minutes and will cycle the heating element. Continue?')) {
                fetch('/api/autotune/start', {method: 'POST'})
                .then(response => response.text())
                .then(data => {
                    alert(data);
                    updateAutotuneStatus();
                });
            }
        }
        
        function stopAutotune() {
            fetch('/api/autotune/stop', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                alert(data);
                updateAutotuneStatus();
            });
        }
        
        function updateAutotuneStatus() {
            fetch('/api/autotune/status')
            .then(response => response.json())
            .then(data => {
                const statusSpan = document.getElementById('autotuneStatus');
                const startBtn = document.getElementById('autotuneBtn');
                const stopBtn = document.getElementById('stopAutotuneBtn');
                
                if (data.running) {
                    statusSpan.innerHTML = `Running... (${data.elapsed}s / ${data.timeout}s)`;
                    statusSpan.style.color = '#ff6600';
                    startBtn.style.display = 'none';
                    stopBtn.style.display = 'inline-block';
                } else {
                    statusSpan.innerHTML = '';
                    startBtn.style.display = 'inline-block';
                    stopBtn.style.display = 'none';
                    
                    // Reload config to get updated PID values
                    loadConfig();
                }
            });
        }
        
        // Update status every 2 seconds
        setInterval(updateStatus, 2000);
        setInterval(updateAutotuneStatus, 2000);
        
        // Load initial config
        loadConfig();
        updateStatus();
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGES_H

