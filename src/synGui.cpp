#include "synGui.h"

#include <WebServer.h>
#include <WiFi.h>

#include "monitor.h"
#include "version.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"

extern TSettings Settings;
extern nvMemory nvMem;

static WebServer synServer(80);
static bool synStarted = false;
static String synPrice = "--";
static String synHeight = "--";
static unsigned long synMarketMs = 0;

static String jsonEscape(const String &in)
{
  String out;
  out.reserve(in.length() + 8);
  for (unsigned i = 0; i < in.length(); i++)
  {
    char c = in[i];
    if (c == '"' || c == '\\')
      out += '\\';
    if (c == '\n' || c == '\r')
      continue;
    out += c;
  }
  return out;
}

static void handleStatus()
{
  mining_data data = getMiningData(millis());
  String json = "{";
  json += "\"hash\":\"" + jsonEscape(data.currentHashRate) + "\",";
  json += "\"shares\":\"" + jsonEscape(data.completedShares) + "\",";
  json += "\"best\":\"" + jsonEscape(data.bestDiff) + "\",";
  json += "\"temp\":\"" + jsonEscape(data.temp) + "\",";
  json += "\"uptime\":\"" + jsonEscape(data.timeMining) + "\",";
  json += "\"time\":\"" + jsonEscape(data.currentTime) + "\",";
  json += "\"price\":\"" + jsonEscape(synPrice) + "\",";
  json += "\"height\":\"" + jsonEscape(synHeight) + "\",";
  json += "\"pool\":\"" + jsonEscape(Settings.PoolAddress) + "\",";
  json += "\"port\":" + String(Settings.PoolPort) + ",";
  json += "\"worker\":\"" + jsonEscape(String(Settings.BtcWallet)) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"version\":\"" + String(CURRENT_VERSION) + "\"";
  json += "}";
  synServer.send(200, "application/json", json);
}

static void handlePool()
{
  String host = synServer.arg("host");
  host.trim();
  String worker = synServer.arg("worker");
  worker.trim();
  int port = synServer.arg("port").toInt();
  if (host.length() == 0 || host.indexOf(' ') >= 0 || worker.length() == 0 || port < 1 || port > 65535)
  {
    synServer.send(400, "text/plain", "Pool host, port, and worker are required.");
    return;
  }
  Settings.PoolAddress = host;
  Settings.PoolPort = port;
  strncpy(Settings.BtcWallet, worker.c_str(), sizeof(Settings.BtcWallet) - 1);
  Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = 0;
  nvMem.saveConfig(&Settings);
  synServer.send(200, "text/html", "<!doctype html><meta http-equiv=\"refresh\" content=\"4;url=/\">"
                                   "<body style=\"background:#07080d;color:#22f0ff;font-family:monospace\">"
                                   "<p>Saved. SynMiner is restarting.</p></body>");
  delay(400);
  ESP.restart();
}

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SynMiner</title>
<style>
:root{--void:#07080d;--cyan:#22f0ff;--yel:#ffe14a;--mag:#ff2ea6;--mute:#8b93a7;--ink:#e8eefc}
*{box-sizing:border-box}body{margin:0;background:var(--void);color:var(--ink);font-family:ui-monospace,monospace}
main{max-width:880px;margin:0 auto;padding:24px}
h1{margin:0;letter-spacing:.2em;color:var(--cyan);font-size:14px}
.row{display:flex;justify-content:space-between;gap:12px;align-items:center}
.hash{margin:28px 0 0;font-size:clamp(64px,12vw,120px);line-height:.9;color:var(--cyan);text-align:center}
.unit{margin:8px 0 0;text-align:center;color:var(--yel);letter-spacing:.28em;font-size:22px}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-top:28px}
.card,.panel{border:1px solid #1c2230;padding:12px}
.k{color:var(--mute);font-size:11px;letter-spacing:.14em}
.v{margin-top:6px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
form{display:grid;gap:10px;margin-top:12px}
input{width:100%;background:#0c1018;color:var(--ink);border:1px solid #2a3348;padding:10px;font:inherit}
button{background:var(--cyan);color:#07080d;border:0;padding:12px;font:inherit;font-weight:700;cursor:pointer}
a{color:var(--cyan)}
.links{display:flex;gap:16px;margin-top:12px}
</style></head><body><main>
<div class="row"><h1>SYNMINER</h1><div id="meta" class="k">--</div></div>
<p class="hash" id="hash">--</p>
<p class="unit">kH/s</p>
<div class="grid">
<div class="card"><div class="k">BEST</div><div class="v" id="best">--</div></div>
<div class="card"><div class="k">SHARES</div><div class="v" id="shares">--</div></div>
<div class="card"><div class="k">HEIGHT</div><div class="v" id="height">--</div></div>
<div class="card"><div class="k">BOARD</div><div class="v" id="temp">--</div></div>
</div>
<div class="grid" style="grid-template-columns:1fr 1fr">
<div class="panel"><div class="k">BTC</div><div class="v" id="price">--</div></div>
<div class="panel"><div class="k">CLOCK</div><div class="v" id="time">--</div></div>
</div>
<section class="panel" style="margin-top:16px">
<div class="k">POOL</div>
<form method="post" action="/api/pool">
<input name="host" id="host" placeholder="stratum host" required>
<input name="port" id="port" placeholder="port" inputmode="numeric" required>
<input name="worker" id="worker" placeholder="worker" required>
<button type="submit">Save pool</button>
</form>
<div class="links">
<a href="https://www.youtube.com/@SyntheticDee" target="_blank" rel="noopener">YouTube SyntheticDee</a>
<a href="https://x.com/Syn_Division" target="_blank" rel="noopener">X Syn_Division</a>
</div>
</section>
</main>
<script>
async function tick(){
  const r=await fetch('/api/status',{cache:'no-store'});
  const d=await r.json();
  hash.textContent=d.hash; best.textContent=d.best; shares.textContent=d.shares;
  height.textContent=d.height; temp.textContent=d.temp+'c'; price.textContent=d.price;
  time.textContent=d.time; meta.textContent=d.ip+'  '+d.version+'  '+d.uptime;
  if(document.activeElement.tagName!=='INPUT'){host.value=d.pool;port.value=d.port;worker.value=d.worker;}
}
tick(); setInterval(tick,2000);
</script></body></html>)HTML";

static void handleRoot()
{
  synServer.send_P(200, "text/html", PAGE);
}

void synGuiStart(void)
{
  if (synStarted || WiFi.status() != WL_CONNECTED)
    return;
  synServer.on("/", HTTP_GET, handleRoot);
  synServer.on("/api/status", HTTP_GET, handleStatus);
  synServer.on("/api/pool", HTTP_POST, handlePool);
  synServer.begin();
  synStarted = true;
  Serial.print("SynMiner GUI http://");
  Serial.println(WiFi.localIP());
}

void synGuiLoop(void)
{
  if (!synStarted)
    return;
  synServer.handleClient();
  if (synMarketMs == 0 || millis() - synMarketMs > 60000)
  {
    synMarketMs = millis();
    clock_data market = getClockData(millis());
    synPrice = market.btcPrice;
    synHeight = market.blockHeight;
  }
}
