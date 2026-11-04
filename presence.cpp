#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::wstring;

static string u8(const wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
  return s;
}
static wstring wide(const string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
  return w;
}
static string lower(const string& s) {
  wstring w = wide(s);
  if (!w.empty()) CharLowerBuffW(&w[0], (DWORD)w.size());
  return u8(w);
}
static string trim(const string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == string::npos ? string() : s.substr(a, b - a + 1);
}
static int64_t ftToMs(FILETIME f) {
  ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime;
  return (int64_t)((u.QuadPart - 116444736000000000ULL) / 10000ULL);
}
static int64_t nowMs() { FILETIME f; GetSystemTimeAsFileTime(&f); return ftToMs(f); }

static bool readFile(const wstring& path, string& out) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER sz{};
  bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (1 << 20);
  if (ok) {
    out.assign((size_t)sz.QuadPart, '\0');
    DWORD got = 0;
    ok = out.empty() || (ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr) && got == out.size());
  }
  CloseHandle(h);
  return ok;
}
static void writeFile(const wstring& path, const string& data) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD w = 0;
  WriteFile(h, data.data(), (DWORD)data.size(), &w, nullptr);
  CloseHandle(h);
}

static std::map<string, string> parseIni(string text) {
  std::map<string, string> m;
  if (text.size() >= 3 && !memcmp(text.data(), "\xEF\xBB\xBF", 3)) text.erase(0, 3);
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    string line = trim(text.substr(pos, nl == string::npos ? string::npos : nl - pos));
    pos = nl == string::npos ? text.size() + 1 : nl + 1;
    if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
    size_t eq = line.find('=');
    if (eq != string::npos) m[lower(trim(line.substr(0, eq)))] = trim(line.substr(eq + 1));
  }
  return m;
}

static wstring g_dir, g_logPath, g_configPath, g_statePath;

static void initPaths(const wstring& dir) {
  g_dir = dir;
  g_logPath = dir + L"\\presence.log";
  g_configPath = dir + L"\\config.ini";
  g_statePath = dir + L"\\state.ini";
}

static void logLine(const string& msg) {
  SYSTEMTIME t; GetSystemTime(&t);
  char ts[40];
  snprintf(ts, sizeof ts, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
           t.wSecond, t.wMilliseconds);
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(g_logPath.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow > 200000)
    MoveFileExW(g_logPath.c_str(), (g_logPath + L".old").c_str(), MOVEFILE_REPLACE_EXISTING);
  HANDLE h = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  string line = ts + msg + "\n";
  DWORD w = 0;
  WriteFile(h, line.data(), (DWORD)line.size(), &w, nullptr);
  CloseHandle(h);
}

static void print(const string& s) {
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!h || h == INVALID_HANDLE_VALUE) {
    if (AttachConsole(ATTACH_PARENT_PROCESS))
      h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  }
  if (!h || h == INVALID_HANDLE_VALUE) return;
  string line = s + "\n";
  DWORD w = 0;
  WriteFile(h, line.data(), (DWORD)line.size(), &w, nullptr);
}

static string jstr(const string& s) {
  string o = "\"";
  for (unsigned char c : s) {
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o + "\"";
}
static string jfield(const string& json, const char* key) {
  string k = string("\"") + key + "\":\"";
  size_t at = json.find(k);
  if (at == string::npos) return {};
  string v;
  for (size_t i = at + k.size(); i < json.size() && json[i] != '"'; i++) {
    if (json[i] == '\\' && i + 1 < json.size()) i++;
    v += json[i];
  }
  return v;
}

struct Button { string label, url; };
struct Config {
  string clientId = "1567196238120484944";
  int pollSeconds = 15;
  string editingText = "Editing {world}";
  string playtestingText = "Playtesting {world}";
  string pickingText = "Picking a world";
  bool showOtherWorlds = true;
  std::vector<string> hiddenWorlds;
  bool hideAllWorldNames = false;
  string hiddenWorldName = "a secret project";
  string timer = "creator";
  string largeImageKey;
  string largeImageText = "TruWorlds Creator";
  string smallImageEditing, smallImagePlaytesting;
  std::vector<Button> buttons;
};

static Config g_cfg;
static int64_t g_cfgStamp = -1;
static void discordClose(bool quiet);
static HANDLE g_discord = INVALID_HANDLE_VALUE;

static void applyIni(const std::map<string, string>& m, Config& c) {
  auto str = [&](const char* k, string& dst) { auto it = m.find(lower(k)); if (it != m.end()) dst = it->second; };
  auto flag = [&](const char* k, bool& dst) {
    auto it = m.find(lower(k));
    if (it == m.end()) return;
    string v = lower(it->second);
    if (v == "true" || v == "yes" || v == "1" || v == "on") dst = true;
    else if (v == "false" || v == "no" || v == "0" || v == "off") dst = false;
  };
  str("clientId", c.clientId);
  string poll;
  str("pollSeconds", poll);
  if (!poll.empty()) c.pollSeconds = atoi(poll.c_str());
  str("editingText", c.editingText);
  str("playtestingText", c.playtestingText);
  str("pickingText", c.pickingText);
  flag("showOtherWorlds", c.showOtherWorlds);
  string hidden;
  str("hiddenWorlds", hidden);
  for (size_t a = 0; a <= hidden.size();) {
    size_t b = hidden.find(',', a);
    if (b == string::npos) b = hidden.size();
    string w = trim(hidden.substr(a, b - a));
    if (!w.empty()) c.hiddenWorlds.push_back(w);
    a = b + 1;
  }
  flag("hideAllWorldNames", c.hideAllWorldNames);
  str("hiddenWorldName", c.hiddenWorldName);
  str("timer", c.timer);
  c.timer = lower(c.timer);
  str("largeImageKey", c.largeImageKey);
  str("largeImageText", c.largeImageText);
  str("smallImageEditing", c.smallImageEditing);
  str("smallImagePlaytesting", c.smallImagePlaytesting);
  for (const char* n : {"1", "2"}) {
    Button b;
    str((string("button") + n + "Label").c_str(), b.label);
    str((string("button") + n + "Url").c_str(), b.url);
    bool http = b.url.rfind("http://", 0) == 0 || b.url.rfind("https://", 0) == 0;
    if (!b.label.empty() && http) c.buttons.push_back(b);
  }
  if (c.pollSeconds < 5) c.pollSeconds = 5;
}

static void loadConfig() {
  WIN32_FILE_ATTRIBUTE_DATA fa;
  int64_t stamp = 0;
  if (GetFileAttributesExW(g_configPath.c_str(), GetFileExInfoStandard, &fa))
    stamp = (int64_t)(((uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime);
  if (stamp == g_cfgStamp) return;
  g_cfgStamp = stamp;
  Config c;
  string text;
  if (stamp && readFile(g_configPath, text)) applyIni(parseIni(text), c);
  bool newApp = c.clientId != g_cfg.clientId;
  g_cfg = c;
  if (newApp && g_discord != INVALID_HANDLE_VALUE) discordClose(false);
  logLine(stamp ? "config loaded" : "config loaded (defaults, no config.ini)");
}

struct Obs { DWORD pid; string title; int64_t started; };
struct Win {
  string world; bool hasWorld = false;
  bool provisional = false;
  string title; bool hasTitle = false;
  bool playing = false;
  int64_t changedAt = 0, startedAt = 0, worldSince = 0, modeSince = 0;
};
struct Activity {
  bool any = false;
  string details, state;
  int64_t start = 0;
  bool playing = false;
};

static const char* PLACEHOLDER = "New World";
static std::map<DWORD, Win> g_windows;
static bool g_firstScan = true;
static string g_stateKey;

static bool isCreatorProcess(DWORD pid, int64_t& started) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return false;
  wchar_t buf[1024];
  DWORD n = 1024;
  bool ok = false;
  if (QueryFullProcessImageNameW(h, 0, buf, &n)) {
    const wchar_t* base = wcsrchr(buf, L'\\');
    ok = _wcsicmp(base ? base + 1 : buf, L"TruWorlds Creator.exe") == 0;
  }
  if (ok) {
    FILETIME c, x, k, u;
    started = GetProcessTimes(h, &c, &x, &k, &u) ? ftToMs(c) : nowMs();
  }
  CloseHandle(h);
  return ok;
}

struct ScanCtx {
  std::map<DWORD, std::pair<bool, int64_t>> procs;
  std::map<DWORD, std::vector<std::pair<int, string>>> titles;
};

static BOOL CALLBACK enumWindow(HWND hw, LPARAM lp) {
  auto* ctx = (ScanCtx*)lp;
  if (GetWindow(hw, GW_OWNER)) return TRUE;
  int len = GetWindowTextLengthW(hw);
  if (len <= 0) return TRUE;
  DWORD pid = 0;
  GetWindowThreadProcessId(hw, &pid);
  auto it = ctx->procs.find(pid);
  if (it == ctx->procs.end()) {
    int64_t st = 0;
    bool c = isCreatorProcess(pid, st);
    it = ctx->procs.emplace(pid, std::make_pair(c, st)).first;
  }
  if (!it->second.first) return TRUE;
  wstring t(len + 1, L'\0');
  t.resize(GetWindowTextW(hw, &t[0], len + 1));
  string title = u8(t);
  int rank = (title.find("TruWorlds Creator") != string::npos ? 0 : 2) + (IsWindowVisible(hw) ? 0 : 1);
  ctx->titles[pid].emplace_back(rank, title);
  return TRUE;
}

static std::vector<Obs> scanWindows() {
  ScanCtx ctx;
  EnumWindows(enumWindow, (LPARAM)&ctx);
  std::vector<Obs> out;
  for (auto& kv : ctx.titles) {
    auto best = std::min_element(kv.second.begin(), kv.second.end(),
                                 [](const auto& a, const auto& b) { return a.first < b.first; });
    out.push_back({kv.first, best->second, ctx.procs[kv.first].second});
  }
  return out;
}

struct Saved { string world; int64_t startedAt; };
static std::map<DWORD, Saved> readState() {
  std::map<DWORD, Saved> m;
  string text;
  if (!readFile(g_statePath, text)) return m;
  for (auto& kv : parseIni(text)) {
    size_t bar = kv.second.find('|');
    if (bar != string::npos)
      m[(DWORD)strtoul(kv.first.c_str(), nullptr, 10)] = {kv.second.substr(bar + 1), _atoi64(kv.second.c_str())};
  }
  return m;
}
static void saveState() {
  string s;
  for (auto& kv : g_windows)
    if (kv.second.hasWorld && !kv.second.provisional)
      s += std::to_string(kv.first) + "=" + std::to_string(kv.second.startedAt) + "|" + kv.second.world + "\n";
  if (s == g_stateKey) return;
  g_stateKey = s;
  writeFile(g_statePath, s);
}

static string shownName(const Win& w) {
  if (!w.hasWorld) return {};
  bool hidden = g_cfg.hideAllWorldNames;
  string lw = lower(w.world);
  for (auto& h : g_cfg.hiddenWorlds) if (lower(h) == lw) hidden = true;
  return hidden ? g_cfg.hiddenWorldName : w.world;
}

static string fit(const string& s) {
  size_t cps = 0;
  for (unsigned char c : s) if ((c & 0xC0) != 0x80) cps++;
  if (cps > 128) {
    string o;
    size_t n = 0;
    for (char ch : s) {
      if (((unsigned char)ch & 0xC0) != 0x80 && n++ == 127) break;
      o += ch;
    }
    return o + "\xE2\x80\xA6";
  }
  string o = s;
  for (; cps < 2; cps++) o += ' ';
  return o;
}
static string fill(string tmpl, const string& world) {
  size_t at = tmpl.find("{world}");
  if (at != string::npos) tmpl.replace(at, 7, world);
  return tmpl;
}

static bool isPlaytestTitle(const string& t) {
  if (t.rfind("Player ", 0) != 0) return false;
  size_t i = 7;
  while (i < t.size() && isdigit((unsigned char)t[i])) i++;
  return i > 7 && t.compare(i, 3, " - ") == 0;
}

static Activity update(const std::vector<Obs>& obs, int64_t now) {
  std::set<DWORD> seen;
  std::vector<DWORD> fresh;
  for (auto& o : obs) {
    seen.insert(o.pid);
    auto it = g_windows.find(o.pid);
    if (it == g_windows.end()) {
      Win w;
      w.changedAt = g_firstScan ? 0 : now;
      w.startedAt = o.started;
      it = g_windows.emplace(o.pid, w).first;
      fresh.push_back(o.pid);
    }
    Win& w = it->second;
    if (w.hasTitle && o.title == w.title) continue;
    bool playing = isPlaytestTitle(o.title);
    bool startScreen = o.title.rfind("TruWorlds Creator", 0) == 0;
    size_t sep = o.title.find(" - TruWorlds Creator");
    if (!playing && !startScreen && sep == string::npos) continue;
    if (w.hasTitle) w.changedAt = now;
    w.title = o.title;
    w.hasTitle = true;
    if (playing != w.playing) { w.playing = playing; w.modeSince = now; }
    if (playing) continue;
    string name;
    if (!startScreen) {
      name = o.title.substr(0, sep);
      if (!name.empty() && name.back() == '*') name.pop_back();
      name = trim(name);
    }
    if (name.empty()) {
      if (w.hasWorld) { w.hasWorld = w.provisional = false; w.world.clear(); w.worldSince = now; }
    } else if (!w.hasWorld || (w.provisional && name != w.world)) {
      w.world = name;
      w.hasWorld = true;
      w.provisional = name == PLACEHOLDER;
      w.worldSince = now;
    }
  }
  g_firstScan = false;
  for (auto it = g_windows.begin(); it != g_windows.end();) it = seen.count(it->first) ? std::next(it) : g_windows.erase(it);
  if (!fresh.empty()) {
    auto saved = readState();
    for (DWORD pid : fresh) {
      Win& w = g_windows[pid];
      w.worldSince = w.modeSince = w.startedAt;
      auto s = saved.find(pid);
      if (s != saved.end() && s->second.startedAt == w.startedAt) {
        w.world = s->second.world;
        w.hasWorld = true;
        w.provisional = false;
      }
    }
  }
  saveState();

  Activity a;
  if (g_windows.empty()) return a;
  const Win* main = nullptr;
  for (auto& kv : g_windows) if (kv.second.playing) { main = &kv.second; break; }
  if (!main)
    for (auto& kv : g_windows) {
      const Win& w = kv.second;
      if (!main || w.changedAt > main->changedAt || (w.changedAt == main->changedAt && w.startedAt > main->startedAt))
        main = &w;
    }
  string name = shownName(*main);
  a.any = true;
  a.playing = main->playing;
  a.details = fit(main->playing ? fill(g_cfg.playtestingText, name.empty() ? "a world" : name)
                  : !name.empty() ? fill(g_cfg.editingText, name) : g_cfg.pickingText);
  if (g_cfg.showOtherWorlds) {
    std::vector<string> others;
    for (auto& kv : g_windows) {
      if (&kv.second == main || !kv.second.hasWorld) continue;
      string n = shownName(kv.second);
      if (std::find(others.begin(), others.end(), n) == others.end()) others.push_back(n);
    }
    if (!others.empty()) {
      string s = "Also open: ";
      for (size_t i = 0; i < others.size(); i++) s += (i ? ", " : "") + others[i];
      a.state = fit(s);
    }
  }
  if (g_cfg.timer == "creator") {
    a.start = INT64_MAX;
    for (auto& kv : g_windows) a.start = std::min(a.start, kv.second.startedAt);
  } else if (g_cfg.timer == "world") a.start = main->worldSince;
  else if (g_cfg.timer == "activity") a.start = main->modeSince;
  return a;
}

static string activityJson(const Activity& a) {
  if (!a.any) return "null";
  string j = "{\"details\":" + jstr(a.details) + ",\"instance\":false";
  if (!a.state.empty()) j += ",\"state\":" + jstr(a.state);
  if (a.start) j += ",\"timestamps\":{\"start\":" + std::to_string(a.start) + "}";
  string assets;
  if (!g_cfg.largeImageKey.empty())
    assets += "\"large_image\":" + jstr(g_cfg.largeImageKey) + ",\"large_text\":" + jstr(g_cfg.largeImageText);
  const string& small = a.playing ? g_cfg.smallImagePlaytesting : g_cfg.smallImageEditing;
  if (!small.empty())
    assets += string(assets.empty() ? "" : ",") + "\"small_image\":" + jstr(small) +
              ",\"small_text\":" + jstr(a.playing ? "Playtesting" : "Editing");
  if (!assets.empty()) j += ",\"assets\":{" + assets + "}";
  if (!g_cfg.buttons.empty()) {
    j += ",\"buttons\":[";
    for (size_t i = 0; i < g_cfg.buttons.size(); i++)
      j += string(i ? "," : "") + "{\"label\":" + jstr(g_cfg.buttons[i].label.substr(0, 32)) +
           ",\"url\":" + jstr(g_cfg.buttons[i].url) + "}";
    j += "]";
  }
  return j + "}";
}

static bool g_ready = false, g_tickNow = false, g_haveLast = false;
static string g_rx, g_lastSent;
static unsigned g_nonce = 0;

static void discordClose(bool quiet) {
  if (g_discord == INVALID_HANDLE_VALUE) return;
  CloseHandle(g_discord);
  g_discord = INVALID_HANDLE_VALUE;
  g_ready = false;
  g_haveLast = false;
  g_rx.clear();
  if (!quiet) logLine("discord disconnected");
}

static void discordSend(int32_t op, const string& body) {
  if (g_discord == INVALID_HANDLE_VALUE) return;
  string frame(8, '\0');
  int32_t len = (int32_t)body.size();
  memcpy(&frame[0], &op, 4);
  memcpy(&frame[4], &len, 4);
  frame += body;
  DWORD w = 0;
  if (!WriteFile(g_discord, frame.data(), (DWORD)frame.size(), &w, nullptr)) discordClose(false);
}

static void discordConnect() {
  for (int i = 0; i < 10; i++) {
    wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
    HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) continue;
    g_discord = h;
    g_rx.clear();
    discordSend(0, "{\"v\":1,\"client_id\":" + jstr(g_cfg.clientId) + "}");
    return;
  }
}

static void discordPump() {
  while (g_discord != INVALID_HANDLE_VALUE) {
    DWORD avail = 0;
    if (!PeekNamedPipe(g_discord, nullptr, 0, nullptr, &avail, nullptr)) { discordClose(false); return; }
    if (!avail) break;
    string chunk(avail, '\0');
    DWORD got = 0;
    if (!ReadFile(g_discord, &chunk[0], avail, &got, nullptr)) { discordClose(false); return; }
    g_rx.append(chunk, 0, got);
  }
  while (g_rx.size() >= 8) {
    int32_t op, len;
    memcpy(&op, g_rx.data(), 4);
    memcpy(&len, g_rx.data() + 4, 4);
    if (len < 0 || g_rx.size() < 8 + (size_t)len) break;
    string body = g_rx.substr(8, len);
    g_rx.erase(0, 8 + len);
    string evt = jfield(body, "evt");
    if (op == 1 && evt == "READY") {
      g_ready = true;
      string user = jfield(body, "username");
      logLine("discord ready (" + (user.empty() ? string("?") : user) + ")");
      g_tickNow = true;
    } else if (op == 1 && evt == "ERROR") {
      logLine("discord error: " + body);
    } else if (op == 2) {
      logLine("discord closed: " + body);
      discordClose(false);
      return;
    } else if (op == 3) {
      discordSend(4, body);
    }
  }
}

static void setActivity(const Activity& a) {
  discordSend(1, "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(GetCurrentProcessId()) +
                     ",\"activity\":" + activityJson(a) + "},\"nonce\":\"" + std::to_string(++g_nonce) + "\"}");
}

static void tick() {
  loadConfig();
  Activity a = update(scanWindows(), nowMs());
  if (g_discord == INVALID_HANDLE_VALUE) { if (a.any) discordConnect(); return; }
  if (!g_ready) return;
  string key = activityJson(a);
  if (g_haveLast && key == g_lastSent) return;
  setActivity(a);
  g_lastSent = key;
  g_haveLast = true;
  logLine(a.any ? "presence: " + a.details + (a.state.empty() ? "" : " | " + a.state) : "presence cleared");
  if (!a.any) discordClose(false);
}

static const wchar_t* LOCK = L"\\\\.\\pipe\\truworlds-creator-rpc";

static int runStop() {
  HANDLE h = CreateFileW(LOCK, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) { print("not running"); return 0; }
  DWORD w = 0;
  WriteFile(h, "stop", 4, &w, nullptr);
  CloseHandle(h);
  for (int i = 0; i < 40 && WaitNamedPipeW(LOCK, 0); i++) Sleep(50);
  Sleep(100);
  print("stopped");
  return 0;
}

static int runDaemon() {
  loadConfig();
  bool idOk = g_cfg.clientId.size() >= 17 && g_cfg.clientId.size() <= 20 &&
              g_cfg.clientId.find_first_not_of("0123456789") == string::npos;
  if (!idOk) { logLine("config.ini: clientId is not a Discord Application ID, exiting"); return 1; }
  HANDLE lock = CreateNamedPipeW(LOCK, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                 PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, 64, 0, nullptr);
  if (lock == INVALID_HANDLE_VALUE) { logLine("already running, exiting"); return 0; }
  OVERLAPPED ov{};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  auto listen = [&] {
    ResetEvent(ov.hEvent);
    if (!ConnectNamedPipe(lock, &ov) && GetLastError() == ERROR_PIPE_CONNECTED) SetEvent(ov.hEvent);
  };
  listen();
  logLine("started (pid " + std::to_string(GetCurrentProcessId()) + ", client " + g_cfg.clientId + ")");
  int64_t next = 0;
  for (;;) {
    int64_t now = nowMs();
    if (g_tickNow || now >= next) {
      g_tickNow = false;
      tick();
      next = now + (int64_t)g_cfg.pollSeconds * 1000;
    }
    discordPump();
    if (WaitForSingleObject(ov.hEvent, 0) == WAIT_OBJECT_0) {
      char buf[16];
      DWORD got = 0, avail = 0;
      Sleep(20);
      if (PeekNamedPipe(lock, nullptr, 0, nullptr, &avail, nullptr) && avail) {
        OVERLAPPED rov{};
        rov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (ReadFile(lock, buf, sizeof buf, &got, &rov) || GetOverlappedResult(lock, &rov, &got, TRUE)) {
          if (got >= 4 && !memcmp(buf, "stop", 4)) {
            if (g_discord != INVALID_HANDLE_VALUE && g_ready) { setActivity(Activity{}); Sleep(200); }
            logLine("stopped");
            return 0;
          }
        }
        CloseHandle(rov.hEvent);
      }
      DisconnectNamedPipe(lock);
      listen();
    }
    Sleep(250);
  }
}

int wmain(int argc, wchar_t** argv) {
  wchar_t exe[1024];
  DWORD n = GetModuleFileNameW(nullptr, exe, 1024);
  wstring dir(exe, n);
  initPaths(dir.substr(0, dir.find_last_of(L'\\')));
  for (int i = 1; i < argc; i++) {
    if (!wcscmp(argv[i], L"--stop")) return runStop();
    if (!wcscmp(argv[i], L"--once")) {
      loadConfig();
      print(activityJson(update(scanWindows(), nowMs())));
      return 0;
    }
  }
  return runDaemon();
}
