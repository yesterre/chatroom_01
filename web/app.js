const statusText = document.getElementById('statusText');
const statusDot = document.getElementById('statusDot');
const nicknameInput = document.getElementById('nickname');
const connectBtn = document.getElementById('connectBtn');
const disconnectBtn = document.getElementById('disconnectBtn');
const messageList = document.getElementById('messageList');
const messageInput = document.getElementById('messageInput');
const sendForm = document.getElementById('sendForm');
const onlineUsers = document.getElementById('onlineUsers');
const onlineCount = document.getElementById('onlineCount');

let connected = false;
let currentNickname = '';
let eventSource = null;

function createSessionId() {
  const rand = Math.random().toString(36).slice(2, 10);
  const now = Date.now().toString(36);
  return 'sid_' + now + '_' + rand;
}

function getSessionId() {
  const key = 'chatroom_web_sid';
  let sid = sessionStorage.getItem(key);
  if (sid === null || sid.length === 0) {
    sid = createSessionId();
    sessionStorage.setItem(key, sid);
  }
  return sid;
}

const sessionId = getSessionId();
const HISTORY_KEY = 'chatroom_history_' + sessionId;
const NICKNAME_KEY = 'chatroom_nickname_' + sessionId;
const MAX_HISTORY = 300;

function loadHistory() {
  const raw = localStorage.getItem(HISTORY_KEY);
  if (raw === null || raw.length === 0) {
    return [];
  }

  try {
    const list = JSON.parse(raw);
    if (Array.isArray(list) === false) {
      return [];
    }
    return list.filter(function (item) {
      return item && typeof item.text === 'string' && typeof item.type === 'string';
    });
  } catch (e) {
    return [];
  }
}

function saveHistory(list) {
  const safeList = Array.isArray(list) ? list.slice(-MAX_HISTORY) : [];
  localStorage.setItem(HISTORY_KEY, JSON.stringify(safeList));
}

function readNicknameCache() {
  const name = localStorage.getItem(NICKNAME_KEY);
  return name && name.length > 0 ? name : '';
}

function writeNicknameCache(name) {
  if (typeof name === 'string' && name.length > 0) {
    localStorage.setItem(NICKNAME_KEY, name);
  }
}

let historyCache = loadHistory();

function appendMessage(text, type, persist) {
  const itemType = type || 'other';
  const shouldPersist = persist !== false;

  const div = document.createElement('div');
  div.className = 'msg ' + itemType;
  div.textContent = text;
  messageList.appendChild(div);
  messageList.scrollTop = messageList.scrollHeight;

  if (shouldPersist) {
    historyCache.push({ text: text, type: itemType, at: Date.now() });
    if (historyCache.length > MAX_HISTORY) {
      historyCache = historyCache.slice(-MAX_HISTORY);
    }
    saveHistory(historyCache);
  }
}

function restoreHistoryToView() {
  historyCache.forEach(function (item) {
    appendMessage(item.text, item.type, false);
  });
}

function setConnected(nextConnected, nickname) {
  const nextName = nickname || '';
  connected = nextConnected;
  currentNickname = nextName;

  if (connected) {
    statusText.textContent = '已连接：' + (nextName || 'unknown');
    statusDot.classList.remove('offline');
    statusDot.classList.add('online');
  } else {
    statusText.textContent = '未连接';
    statusDot.classList.remove('online');
    statusDot.classList.add('offline');
  }

  connectBtn.disabled = connected;
  disconnectBtn.disabled = connected === false;
  messageInput.disabled = connected === false;
}

function renderUsers(users) {
  while (onlineUsers.firstChild) {
    onlineUsers.removeChild(onlineUsers.firstChild);
  }

  const list = Array.isArray(users) ? users : [];
  onlineCount.textContent = String(list.length);

  if (list.length === 0) {
    const li = document.createElement('li');
    li.textContent = '暂无';
    onlineUsers.appendChild(li);
    return;
  }

  list.forEach(function (name) {
    const li = document.createElement('li');
    li.textContent = name;
    onlineUsers.appendChild(li);
  });
}

function parseJsonSafely(text) {
  try {
    return JSON.parse(text);
  } catch (e) {
    return null;
  }
}

function ensureEventStream() {
  if (eventSource !== null) {
    return;
  }

  const url = '/api/events?sid=' + encodeURIComponent(sessionId);
  eventSource = new EventSource(url);

  eventSource.addEventListener('status', function (event) {
    const data = parseJsonSafely(event.data);
    if (data === null) {
      return;
    }

    setConnected(Boolean(data.connected), data.nickname || '');

    if (data.connected && typeof data.nickname === 'string' && data.nickname.length > 0) {
      writeNicknameCache(data.nickname);
      nicknameInput.value = data.nickname;
    }
  });

  eventSource.addEventListener('users', function (event) {
    const data = parseJsonSafely(event.data);
    if (data === null) {
      return;
    }
    renderUsers(data.users || []);
  });

  eventSource.addEventListener('message', function (event) {
    const data = parseJsonSafely(event.data);
    if (data === null) {
      return;
    }
    if (typeof data.text !== 'string' || data.text.length === 0) {
      return;
    }
    appendMessage(data.text, 'other', true);
  });

  eventSource.addEventListener('system', function (event) {
    const data = parseJsonSafely(event.data);
    if (data && typeof data.text === 'string') {
      appendMessage(data.text, 'system', true);
    } else {
      appendMessage(event.data, 'system', true);
    }
  });

  eventSource.onerror = function () {
    setConnected(false, '');
  };
}

async function postJson(url, body) {
  const payload = Object.assign({}, body, { sid: sessionId });

  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });

  let data;
  try {
    data = await res.json();
  } catch (e) {
    data = { ok: false, error: 'bad json response' };
  }

  if (res.ok === false || data.ok !== true) {
    throw new Error(data.error || ('request failed: ' + res.status));
  }

  return data;
}

async function connectWithNickname(nickname) {
  const name = (nickname || '').trim();
  if (name.length === 0) {
    appendMessage('请输入昵称后再连接。', 'system', true);
    return;
  }

  try {
    ensureEventStream();
    const data = await postJson('/api/connect', { nickname: name });
    const finalName = data.nickname || name;
    setConnected(true, finalName);
    writeNicknameCache(finalName);
    nicknameInput.value = finalName;
    appendMessage('你已进入聊天室：' + finalName, 'system', true);
  } catch (err) {
    appendMessage('连接失败：' + err.message, 'system', true);
  }
}

connectBtn.addEventListener('click', async function () {
  await connectWithNickname(nicknameInput.value);
});

disconnectBtn.addEventListener('click', async function () {
  if (connected === false) {
    return;
  }

  try {
    await postJson('/api/disconnect', {});
    setConnected(false, '');
    appendMessage('已断开连接。', 'system', true);
  } catch (err) {
    appendMessage('断开失败：' + err.message, 'system', true);
  }
});

sendForm.addEventListener('submit', async function (event) {
  event.preventDefault();

  const text = messageInput.value.trim();
  if (text.length === 0) {
    return;
  }

  if (connected === false) {
    appendMessage('请先连接后端。', 'system', true);
    return;
  }

  try {
    await postJson('/api/send', { message: text });
    appendMessage('[' + currentNickname + '] says: ' + text, 'self', true);
    messageInput.value = '';
  } catch (err) {
    appendMessage('发送失败：' + err.message, 'system', true);
  }
});

setConnected(false, '');
renderUsers([]);
restoreHistoryToView();

const cachedNickname = readNicknameCache();
if (cachedNickname.length > 0) {
  nicknameInput.value = cachedNickname;
}

ensureEventStream();

if (cachedNickname.length > 0) {
  setTimeout(function () {
    if (connected === false) {
      connectWithNickname(cachedNickname);
    }
  }, 200);
}

if (historyCache.length === 0) {
  appendMessage('页面已就绪，先输入昵称并连接。', 'system', true);
}
