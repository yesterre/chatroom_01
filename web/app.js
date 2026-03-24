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

function appendMessage(text, type) {
  const div = document.createElement('div');
  const itemType = type || 'other';
  div.className = 'msg ' + itemType;
  div.textContent = text;
  messageList.appendChild(div);
  messageList.scrollTop = messageList.scrollHeight;
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
    appendMessage(data.text, 'other');
  });

  eventSource.addEventListener('system', function (event) {
    const data = parseJsonSafely(event.data);
    if (data && typeof data.text === 'string') {
      appendMessage(data.text, 'system');
    } else {
      appendMessage(event.data, 'system');
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

connectBtn.addEventListener('click', async function () {
  const nickname = nicknameInput.value.trim();
  if (nickname.length === 0) {
    appendMessage('请输入昵称后再连接。', 'system');
    return;
  }

  try {
    ensureEventStream();
    const data = await postJson('/api/connect', { nickname: nickname });
    setConnected(true, data.nickname || nickname);
    appendMessage('你已进入聊天室：' + (data.nickname || nickname), 'system');
  } catch (err) {
    appendMessage('连接失败：' + err.message, 'system');
  }
});

disconnectBtn.addEventListener('click', async function () {
  if (connected === false) {
    return;
  }

  try {
    await postJson('/api/disconnect', {});
    setConnected(false, '');
    appendMessage('已断开连接。', 'system');
  } catch (err) {
    appendMessage('断开失败：' + err.message, 'system');
  }
});

sendForm.addEventListener('submit', async function (event) {
  event.preventDefault();

  const text = messageInput.value.trim();
  if (text.length === 0) {
    return;
  }

  if (connected === false) {
    appendMessage('请先连接后端。', 'system');
    return;
  }

  try {
    await postJson('/api/send', { message: text });
    appendMessage('[' + currentNickname + '] says: ' + text, 'self');
    messageInput.value = '';
  } catch (err) {
    appendMessage('发送失败：' + err.message, 'system');
  }
});

setConnected(false, '');
renderUsers([]);
ensureEventStream();
appendMessage('页面已就绪，先输入昵称并连接。', 'system');
