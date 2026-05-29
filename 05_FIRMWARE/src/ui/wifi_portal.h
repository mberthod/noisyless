// wifi_portal.h — Portail captif WiFiManager customisé NOISYLESS
// Reprend l'identité visuelle du site noisyless.com (Space Mono, minimal)
#pragma once

#include <Arduino.h>
#include <WiFiManager.h>
#include <functional>

// Paramètres du portail : SSID, mot de passe fallback
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// Fonctions externes pour NVS
extern void saveWiFiCredentials(const String& ssid, const String& pass);
extern bool hasSavedCredentials();
extern String getSavedSSID();
extern String getSavedPass();
extern void startWiFiFallback();

// HTML custom complet pour le portail — identité Noisyless
static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>NOISYLESS — Configuration WiFi</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=IBM+Plex+Sans:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'IBM Plex Sans', -apple-system, sans-serif;
      background: #ffffff;
      color: #1a1a1a;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 24px;
    }
    .container {
      max-width: 420px;
      width: 100%;
    }
    .logo {
      font-family: 'Space Mono', monospace;
      font-size: 22px;
      font-weight: 700;
      letter-spacing: 2px;
      text-transform: uppercase;
      text-align: center;
      margin-bottom: 8px;
      color: #1a1a1a;
    }
    .subtitle {
      font-size: 14px;
      color: #666;
      text-align: center;
      margin-bottom: 32px;
      line-height: 1.5;
    }
    .card {
      background: #fff;
      border: 1px solid #e0e0e0;
      border-radius: 12px;
      padding: 32px 24px;
    }
    h2 {
      font-family: 'Space Mono', monospace;
      font-size: 16px;
      font-weight: 700;
      margin-bottom: 20px;
      text-align: center;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .form-group {
      margin-bottom: 16px;
    }
    label {
      display: block;
      font-size: 12px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 1px;
      color: #666;
      margin-bottom: 6px;
    }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 14px 16px;
      border: 1px solid #d0d0d0;
      border-radius: 8px;
      font-family: 'IBM Plex Sans', sans-serif;
      font-size: 15px;
      color: #1a1a1a;
      background: #fafafa;
      transition: border-color 0.2s;
    }
    input[type="text"]:focus, input[type="password"]:focus {
      outline: none;
      border-color: #1a1a1a;
      background: #fff;
    }
    .btn {
      width: 100%;
      padding: 14px;
      background: #1a1a1a;
      color: #fff;
      border: none;
      border-radius: 8px;
      font-family: 'Space Mono', monospace;
      font-size: 14px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 1px;
      cursor: pointer;
      transition: background 0.2s;
      margin-top: 8px;
    }
    .btn:hover {
      background: #333;
    }
    .btn:active {
      background: #000;
    }
    .status {
      margin-top: 16px;
      padding: 12px;
      border-radius: 8px;
      font-size: 13px;
      text-align: center;
      display: none;
    }
    .status.success {
      background: #e8f5e9;
      color: #2e7d32;
      border: 1px solid #c8e6c9;
      display: block;
    }
    .status.error {
      background: #fbe9e7;
      color: #c62828;
      border: 1px solid #ffccbc;
      display: block;
    }
    .status.info {
      background: #e3f2fd;
      color: #1565c0;
      border: 1px solid #bbdefb;
      display: block;
    }
    .info-text {
      font-size: 12px;
      color: #999;
      text-align: center;
      margin-top: 16px;
      line-height: 1.6;
    }
    .spinner {
      display: none;
      width: 20px;
      height: 20px;
      border: 2px solid #e0e0e0;
      border-top: 2px solid #1a1a1a;
      border-radius: 50%;
      animation: spin 0.8s linear infinite;
      margin: 12px auto;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
    .footer {
      text-align: center;
      margin-top: 24px;
      font-size: 11px;
      color: #aaa;
      font-family: 'Space Mono', monospace;
    }
    .otp-hint {
      font-size: 12px;
      color: #888;
      text-align: center;
      margin-top: 8px;
      border-top: 1px solid #eee;
      padding-top: 12px;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="logo">NOISYLESS</div>
    <div class="subtitle">Connectez votre capteur au WiFi</div>

    <div class="card">
      <h2>Configuration WiFi</h2>

      <div id="scan-status" class="status info" style="display:none;"></div>

      <div class="form-group">
        <label for="ssid">R&eacute;seau WiFi</label>
        <input type="text" id="ssid" name="ssid" placeholder="Nom du réseau..." list="networks" autocomplete="off">
        <datalist id="networks"></datalist>
      </div>

      <div class="form-group">
        <label for="password">Mot de passe</label>
        <input type="password" id="password" name="password" placeholder="••••••••">
      </div>

      <button class="btn" id="connect-btn" onclick="connectWiFi()">Connecter</button>

      <div id="status-msg" class="status"></div>
      <div id="spinner" class="spinner"></div>

      <div class="info-text">
        Le capteur red&eacute;marrera automatiquement apr&egrave;s connexion.<br>
        Configuration unique — le r&eacute;seau est m&eacute;moris&eacute;.
      </div>
    </div>

    <div class="footer">NOISYLESS &mdash; Capteurs intelligents pour l'immobilier</div>
  </div>

  <script>
    function connectWiFi() {
      var ssid = document.getElementById('ssid').value.trim();
      var pass = document.getElementById('password').value.trim();
      var msg = document.getElementById('status-msg');
      var spinner = document.getElementById('spinner');
      var btn = document.getElementById('connect-btn');

      if (!ssid) {
        msg.className = 'status error';
        msg.textContent = 'Veuillez saisir un nom de réseau WiFi';
        msg.style.display = 'block';
        return;
      }

      btn.disabled = true;
      btn.textContent = 'Connexion...';
      spinner.style.display = 'block';
      msg.style.display = 'none';

      fetch('/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
      })
      .then(function(r) { return r.text(); })
      .then(function(text) {
        spinner.style.display = 'none';
        if (text.indexOf('OK') >= 0) {
          msg.className = 'status success';
          msg.textContent = '✓ Connexion réussie ! Redémarrage...';
          msg.style.display = 'block';
          setTimeout(function() { window.location.href = '/'; }, 2000);
        } else if (text.indexOf('FAIL') >= 0) {
          msg.className = 'status error';
          msg.textContent = '✗ Échec de connexion. Vérifiez le mot de passe.';
          msg.style.display = 'block';
          btn.disabled = false;
          btn.textContent = 'Réessayer';
        } else {
          msg.className = 'status error';
          msg.textContent = '✗ Erreur : ' + text;
          msg.style.display = 'block';
          btn.disabled = false;
          btn.textContent = 'Réessayer';
        }
      })
      .catch(function(err) {
        spinner.style.display = 'none';
        msg.className = 'status error';
        msg.textContent = '✗ Erreur réseau : ' + err;
        msg.style.display = 'block';
        btn.disabled = false;
        btn.textContent = 'Réessayer';
      });
    }

    // Scan WiFi networks on load
    fetch('/scan')
      .then(function(r) { return r.json(); })
      .then(function(networks) {
        var list = document.getElementById('networks');
        var status = document.getElementById('scan-status');
        if (networks.length > 0) {
          var ssid = document.getElementById('ssid');
          networks.sort(function(a, b) { return b.rssi - a.rssi; });
          networks.forEach(function(n) {
            var opt = document.createElement('option');
            opt.value = n.ssid;
            list.appendChild(opt);
          });
          status.textContent = networks.length + ' réseau(x) trouvé(s)';
          status.style.display = 'block';
          setTimeout(function() { status.style.display = 'none'; }, 4000);
        } else {
          status.textContent = 'Aucun réseau trouvé. Vérifiez que le WiFi est allumé.';
          status.className = 'status error';
          status.style.display = 'block';
        }
      })
      .catch(function(err) {
        console.log('Scan error: ' + err);
      });

    // Auto-fill password field on Enter
    document.getElementById('password').addEventListener('keydown', function(e) {
      if (e.key === 'Enter') connectWiFi();
    });
    document.getElementById('ssid').addEventListener('keydown', function(e) {
      if (e.key === 'Enter') document.getElementById('password').focus();
    });
  </script>
</body>
</html>
)rawliteral";

// Paramètres du portail personnalisé
static const char* PORTAL_TITLE = "NOISYLESS";
static const uint16_t PORTAL_TIMEOUT_SEC = 300;  // 5 minutes max

// Set up WiFiManager with custom portal
void setupCustomPortal(WiFiManager& wm) {
  wm.setTitle(PORTAL_TITLE);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_SEC);
  wm.setCustomHeadElement(PSTR("<style>body{font-family:'IBM Plex Sans',sans-serif}</style>"));

  // No scan needed — we use our own JS fetch('/scan')
}

// Start captive portal with custom HTML
bool startCaptivePortal(WiFiManager& wm) {
  // Use custom parameters
  WiFiManagerParameter custom_ssid("ssid", "Réseau WiFi", "", 64);
  WiFiManagerParameter custom_pass("pass", "Mot de passe", "", 64);

  wm.addParameter(&custom_ssid);
  wm.addParameter(&custom_pass);

  // The portal HTML is served by WiFiManager at / (root)
  // We customize via wm.setCustomHeadElement and CSS overrides
  return wm.autoConnect("NOISYLESS-Setup");
}
