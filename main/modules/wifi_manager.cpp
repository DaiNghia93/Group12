#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "modules/fan_speed.h"

static const char *TAG = "wifi_manager";

static const char* ssid = "abc";
static const char* password = "2444666668888888";

static httpd_handle_t server = NULL;

// ===== STATE =====
static bool wifi_connected = false;
static bool fanActive = true;
static int fanSpeedLevel = 3;
static bool isManual = false;
bool wifi_is_connected() {
    return wifi_connected;
}
// ===== GET STATE =====
bool wifi_is_manual_mode() { return isManual; }
int wifi_get_speed_level() { return fanSpeedLevel; }
bool wifi_get_fan_state() { return fanActive; }

// ===== HTML =====
static const char* getHTML() {
  return R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Fan Pro</title>
    <style>
      :root {
        --primary: #00d2ff; --secondary: #3a7bd5;
        --bg: #0f172a; --card: rgba(255, 255, 255, 0.1);
        --text: #f8fafc; --success: #22c55e; --danger: #ef4444;
      }
      body {
        font-family: 'Segoe UI', sans-serif;
        background: linear-gradient(135deg, #0f172a 0%, #1e293b 100%);
        color: var(--text); display: flex; justify-content: center;
        align-items: center; min-height: 100vh; margin: 0;
      }
      .card {
        background: var(--card); backdrop-filter: blur(15px);
        padding: 2.5rem; border-radius: 2.5rem; width: 340px;
        box-shadow: 0 25px 50px -12px rgba(0,0,0,0.5);
        border: 1px solid rgba(255,255,255,0.1); text-align: center;
      }
      .fan-icon {
        font-size: 60px; margin: 25px 0; transition: all 0.3s ease;
        display: inline-block; filter: drop-shadow(0 0 10px var(--primary));
      }
      .spinning { animation: spin 2s linear infinite; }
      @keyframes spin { from {transform: rotate(0deg);} to {transform: rotate(360deg);} }
      
      .status-badge {
        padding: 6px 12px; border-radius: 20px; font-size: 0.75rem;
        font-weight: 800; letter-spacing: 1px;
      }
      .bg-auto { background: var(--secondary); box-shadow: 0 0 10px var(--secondary); }
      .bg-manual { background: #f59e0b; box-shadow: 0 0 10px #f59e0b; }

      .btn-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; margin: 20px 0; }
      button {
        padding: 12px; border: none; border-radius: 15px;
        background: rgba(255,255,255,0.05); color: white;
        cursor: pointer; transition: 0.3s; border: 1px solid rgba(255,255,255,0.1);
        font-weight: bold;
      }
      button:hover { background: rgba(255,255,255,0.2); transform: translateY(-3px); }
      button.active { 
        background: var(--primary); 
        box-shadow: 0 0 20px var(--primary);
        border: 1px solid white;
      }
      
      .power-btn { 
        width: 100%; padding: 15px; font-weight: 900; 
        margin-bottom: 10px; font-size: 1rem; border-radius: 15px;
        letter-spacing: 1px;
      }
      .on { background: var(--success); box-shadow: 0 0 15px var(--success); } 
      .off { background: var(--danger); box-shadow: 0 0 15px var(--danger); }

      .mode-box {
        display: flex; justify-content: space-between; align-items: center;
        background: rgba(0,0,0,0.3); padding: 12px 20px; border-radius: 18px; margin: 20px 0;
      }
      .switch { position: relative; display: inline-block; width: 45px; height: 24px; }
      .switch input { opacity: 0; width: 0; height: 0; }
      .slider {
        position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
        background-color: #475569; transition: .4s; border-radius: 24px;
      }
      .slider:before {
        position: absolute; content: ""; height: 18px; width: 18px;
        left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%;
      }
      input:checked + .slider { background-color: var(--success); }
      input:checked + .slider:before { transform: translateX(21px); }
    </style>
  </head>
  <body>
    <div class="card">
      <div style="display:flex; justify-content: space-between; align-items: center;">
        <h2 style="margin:0; font-size: 1.2rem; color: var(--primary);">FAN CONTROL</h2>
        <span id="mode-badge" class="status-badge bg-auto">AUTO</span>
      </div>

      <div id="fan-ui" class="fan-icon">🌀</div>
      
      <div class="mode-box">
        <span style="font-size: 0.9rem; font-weight: 600;">Manual Override</span>
        <label class="switch">
          <input type="checkbox" id="modeToggle" onchange="toggleMode()">
          <span class="slider"></span>
        </label>
      </div>

      <button id="pwr-btn" class="power-btn off" onclick="setFan()">TURN OFF</button>

      <div class="btn-grid">
        <button class="speed-btn" onclick="setSpeed(1)">L1</button>
        <button class="speed-btn" onclick="setSpeed(2)">L2</button>
        <button class="speed-btn" onclick="setSpeed(3)">L3</button>
        <button class="speed-btn" onclick="setSpeed(4)">L4</button>
        <button class="speed-btn" onclick="setSpeed(5)">L5</button>
        <button class="speed-btn" onclick="setSpeed(6)">L6</button>
      </div>

      <div style="background: rgba(0,0,0,0.2); padding: 10px; border-radius: 12px;">
        <p id="info-text" style="color: #94a3b8; font-size: 0.85rem; margin:0;">Cảm biến: --°C | --/6</p>
      </div>
    </div>

    <script>
      let fanOn = true;

      function setFan() {
        fanOn = !fanOn;
        fetch('/fan?state=' + (fanOn ? 1 : 0));
        updateUI();
      }

      function setSpeed(v) { fetch('/speed?val=' + v); }
      function toggleMode() { fetch('/mode'); }

      function updateUI() {
        fetch('/status').then(r => r.json()).then(d => {
          const fanIcon = document.getElementById("fan-ui");
          const pwrBtn = document.getElementById("pwr-btn");
          const modeBadge = document.getElementById("mode-badge");

          if(d.fan) {
            fanIcon.classList.add("spinning");
            fanIcon.style.animationDuration = (2.5 / d.speed) + "s";
            pwrBtn.innerText = "TURN OFF FAN";
            pwrBtn.className = "power-btn off";
          } else {
            fanIcon.classList.remove("spinning");
            pwrBtn.innerText = "TURN ON FAN";
            pwrBtn.className = "power-btn on";
          }

          modeBadge.innerText = d.manual ? "MANUAL" : "AUTO";
          modeBadge.className = "status-badge " + (d.manual ? "bg-manual" : "bg-auto");
          document.getElementById("modeToggle").checked = d.manual;

          document.querySelectorAll('.speed-btn').forEach((btn, idx) => {
            const disabled = !d.manual;
            btn.disabled = disabled;
            btn.style.opacity = disabled ? 0.4 : 1;
            btn.classList.toggle('active', (idx + 1) === d.speed && d.fan);
          });

          document.getElementById("info-text").innerHTML = 
            `Nhiệt độ: <b>${d.temp}°C</b> | Mức hiện tại: <b>${d.speed}/6</b>`;
        });
      }
      
      setInterval(updateUI, 1000);
      updateUI();
    </script>
  </body>
  </html>
  )rawliteral";
}

// ===== HANDLERS =====
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_send(req, getHTML(), strlen(getHTML()));
    return ESP_OK;
}

static esp_err_t fan_handler(httpd_req_t *req) {
    if (!isManual) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "AUTO MODE");
        return ESP_OK;
    }
    char buf[32];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "state", param, sizeof(param)) == ESP_OK) {
            fanActive = atoi(param);
            if (fanActive && fanSpeedLevel == 0) fanSpeedLevel = 1;
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t speed_handler(httpd_req_t *req) {
    if (!isManual) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "AUTO MODE");
        return ESP_OK;
    }
    char buf[32];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "val", param, sizeof(param)) == ESP_OK) {
            fanSpeedLevel = atoi(param);
            if (fanSpeedLevel < 0) fanSpeedLevel = 0;
            if (fanSpeedLevel > 6) fanSpeedLevel = 6;
            fanActive = (fanSpeedLevel > 0);
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t mode_handler(httpd_req_t *req) {
    isManual = !isManual;
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    float temp = fan_get_temperature();
    char json[128];
    snprintf(json, sizeof(json), "{\"fan\":%s,\"speed\":%d,\"manual\":%s,\"temp\":%.1f}",
             fanActive ? "true" : "false", fanSpeedLevel, isManual ? "true" : "false", temp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

// ===== WIFI EVENT =====
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// ===== INIT =====
void wifi_manager_init(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t fan_uri = {
        .uri       = "/fan",
        .method    = HTTP_GET,
        .handler   = fan_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &fan_uri);

    httpd_uri_t speed_uri = {
        .uri       = "/speed",
        .method    = HTTP_GET,
        .handler   = speed_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &speed_uri);

    httpd_uri_t mode_uri = {
        .uri       = "/mode",
        .method    = HTTP_GET,
        .handler   = mode_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &mode_uri);

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &status_uri);

    ESP_LOGI(TAG, "WiFi manager initialized");
}

// ===== TASK =====
void wifi_manager_task(void *pv) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

