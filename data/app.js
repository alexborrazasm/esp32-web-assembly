const term = new Terminal({ convertEol: false, disableStdin: true, cursorBlink: false, fontSize: 14, scrollback: 5000 });
const fit = new FitAddon.FitAddon();
term.loadAddon(fit);
term.open(document.getElementById("term"));
fit.fit();
window.addEventListener("resize", () => fit.fit());

const $ = (id) => document.getElementById(id);

// --- Console (WebSocket) -------------------------------------------------
let ws;
let retryTimer;

function setConnected(on) {
  $("conn").textContent = on ? "connected" : "disconnected";
  $("conn").className = "badge " + (on ? "on" : "off");
  $("line").disabled = !on;
}

function connect() {
  clearTimeout(retryTimer);
  const sock = new WebSocket(`ws://${location.host}/console`);
  ws = sock;
  sock.onopen = () => {
    setConnected(true);
    term.reset(); // the board resends its history on connect
  };
  sock.onmessage = (e) => term.write(e.data);
  sock.onclose = () => {
    if (sock !== ws) return; // superseded by reconnect()
    setConnected(false);
    retryTimer = setTimeout(connect, 1500);
  };
}

// A board that reboots does not close its sockets, so the browser can keep a dead
// one open for minutes. Drop it by hand and start over.
function reconnect() {
  if (ws) { ws.onclose = null; ws.onmessage = null; ws.close(); }
  setConnected(false);
  connect();
}
connect();

// --- Command line --------------------------------------------------------
const cmdHistory = [];
let cmdPos = 0;

$("cmd").onsubmit = (e) => {
  e.preventDefault();
  const text = $("line").value;
  if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(text + "\n");
  if (cmdHistory[cmdHistory.length - 1] !== text) cmdHistory.push(text);
  cmdPos = cmdHistory.length;
  $("line").value = "";
};

$("line").onkeydown = (e) => {
  if (e.key === "ArrowUp" && cmdPos > 0) {
    $("line").value = cmdHistory[--cmdPos];
    e.preventDefault();
  } else if (e.key === "ArrowDown") {
    $("line").value = ++cmdPos < cmdHistory.length ? cmdHistory[cmdPos] : "";
    cmdPos = Math.min(cmdPos, cmdHistory.length);
    e.preventDefault();
  }
};

// --- Controls ------------------------------------------------------------
for (const action of ["run", "stop", "clear", "reset"]) {
  $(action).onclick = async () => {
    try { await fetch(`/api/${action}`, { method: "POST" }); } catch {}
    if (action === "reset") setTimeout(reconnect, 1000); // after the board went down
    $("line").focus();
  };
}

// --- .wasm upload ----------------------------------------------------
function setMsg(text, cls) { $("msg").textContent = text; $("msg").className = cls || ""; }

function upload(file) {
  if (!file) return;
  const form = new FormData();
  form.append("module", file, file.name);

  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/upload");
  xhr.upload.onprogress = (e) => {
    if (!e.lengthComputable) return;
    $("bar").hidden = false;
    $("bar").value = (e.loaded / e.total) * 100;
  };
  xhr.onload = () => {
    $("bar").hidden = true;
    let res = {};
    try { res = JSON.parse(xhr.responseText); } catch {}
    if (xhr.status === 200) setMsg(`${file.name}: ${res.size} bytes uploaded`, "ok");
    else setMsg(`Error: ${res.error || xhr.status}`, "err");
    refreshStatus();
  };
  xhr.onerror = () => { $("bar").hidden = true; setMsg("Network error", "err"); };
  setMsg(`Uploading ${file.name}...`);
  xhr.send(form);
}

const drop = $("drop");
$("pick").onclick = () => $("file").click();
$("file").onchange = (e) => { upload(e.target.files[0]); e.target.value = ""; };
drop.ondragover = (e) => { e.preventDefault(); drop.classList.add("over"); };
drop.ondragleave = () => drop.classList.remove("over");
drop.ondrop = (e) => {
  e.preventDefault();
  drop.classList.remove("over");
  upload(e.dataTransfer.files[0]);
};

// --- Status --------------------------------------------------------------
let lastUptime = 0;
async function refreshStatus() {
  try {
    const s = await (await fetch("/api/status")).json();
    if (s.uptime < lastUptime) reconnect(); // rebooted since the last poll
    lastUptime = s.uptime;
    $("info").textContent =
      `${s.mode.toUpperCase()} ${s.ssid} · ${s.ip} · heap ${(s.heap / 1024) | 0}K · psram ${(s.psram / 1048576).toFixed(1)}M · module ${s.module} B`;
  } catch {
    // board unreachable: make sure we are not holding on to a dead socket
    if (ws && ws.readyState === WebSocket.OPEN) reconnect();
  }
}
refreshStatus();
setInterval(refreshStatus, 5000);
