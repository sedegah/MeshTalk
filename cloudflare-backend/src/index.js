/** @typedef {{ DB: D1Database }} Env */

const json = (data, init = {}) =>
  new Response(JSON.stringify(data, null, 2), {
    headers: { "content-type": "application/json; charset=utf-8" },
    ...init,
  });

const html = (content) =>
  new Response(content, {
    headers: { "content-type": "text/html; charset=utf-8" },
  });

const badRequest = (message) => json({ error: message }, { status: 400 });
const notFound = () => json({ error: "Not found" }, { status: 404 });

async function readBody(request) {
  try {
    return await request.json();
  } catch {
    return null;
  }
}

function isNonEmptyString(v) {
  return typeof v === "string" && v.trim().length > 0;
}

function homepage() {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>MeshTalk Bootstrap API</title>
  <style>
    :root { --bg:#0b1020; --card:#111a33; --muted:#8fa5d6; --text:#eef3ff; --accent:#5cc8ff; --ok:#47d18c; }
    body{margin:0;font-family:Inter,system-ui,-apple-system,sans-serif;background:radial-gradient(circle at 20% 0%,#1a2a52, #0b1020 55%);color:var(--text)}
    .wrap{max-width:960px;margin:0 auto;padding:32px 16px 56px}
    .hero{padding:24px;border:1px solid #2a3b6b;border-radius:16px;background:rgba(17,26,51,.92);backdrop-filter: blur(6px)}
    h1{margin:0 0 8px;font-size:1.9rem}.muted{color:var(--muted)}
    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:14px;margin-top:18px}
    .card{border:1px solid #263963;border-radius:12px;padding:14px;background:#0f1730}
    code{background:#0a1228;color:var(--accent);padding:2px 6px;border-radius:6px}
    .badge{display:inline-block;background:rgba(71,209,140,.15);color:var(--ok);padding:4px 10px;border-radius:999px;font-size:.82rem;margin-bottom:8px}
    ul{padding-left:18px}.footer{margin-top:16px;color:var(--muted);font-size:.9rem}
  </style>
</head>
<body>
<div class="wrap">
  <section class="hero">
    <span class="badge">MeshTalk Network v2</span>
    <h1>Bootstrap Backend API</h1>
    <p class="muted">Cloudflare Worker + D1 service for peer discovery, presence, offline sync, and multi-device key storage.</p>
    <div class="grid">
      <div class="card"><strong>Health</strong><ul><li><code>GET /health</code></li></ul></div>
      <div class="card"><strong>Peers</strong><ul><li><code>POST /api/register</code></li><li><code>POST /api/heartbeat</code></li><li><code>GET /api/peers</code></li></ul></div>
      <div class="card"><strong>Offline Queue</strong><ul><li><code>POST /api/offline/messages</code></li><li><code>GET /api/offline/messages/:username?clear=true</code></li></ul></div>
      <div class="card"><strong>Device Keys</strong><ul><li><code>PUT /api/device-key</code></li><li><code>GET /api/device-key/:username</code></li></ul></div>
    </div>
    <p class="footer">D1 schema source: <code>cloudflare-backend/schema.sql</code></p>
  </section>
</div>
</body>
</html>`;
}

async function logAudit(env, method, path, statusCode, actor = null) {
  await env.DB.prepare(
    `INSERT INTO api_audit_log (method, path, status_code, actor) VALUES (?1, ?2, ?3, ?4)`
  )
    .bind(method, path, statusCode, actor)
    .run();
}

async function registerPeer(env, body) {
  if (!isNonEmptyString(body?.username) || !isNonEmptyString(body?.publicKey) || !isNonEmptyString(body?.address)) {
    return badRequest("username, publicKey and address are required");
  }

  const username = body.username.trim().toLowerCase();
  const now = new Date().toISOString();

  await env.DB.prepare(
    `INSERT INTO peers (username, public_key, address, port, device_id, status, last_seen, updated_at)
     VALUES (?1, ?2, ?3, ?4, ?5, 'online', ?6, ?6)
     ON CONFLICT(username) DO UPDATE SET
      public_key=excluded.public_key,
      address=excluded.address,
      port=excluded.port,
      device_id=excluded.device_id,
      status='online',
      last_seen=excluded.last_seen,
      updated_at=excluded.updated_at`
  )
    .bind(username, body.publicKey, body.address, body.port ?? null, body.deviceId ?? null, now)
    .run();

  const peer = await env.DB.prepare(`SELECT username, public_key as publicKey, address, port, device_id as deviceId, status, last_seen as lastSeen FROM peers WHERE username=?1`)
    .bind(username)
    .first();

  return json({ ok: true, peer });
}

async function heartbeat(env, body) {
  if (!isNonEmptyString(body?.username)) return badRequest("username is required");
  const username = body.username.trim().toLowerCase();
  const now = new Date().toISOString();

  const result = await env.DB.prepare(
    `UPDATE peers SET status='online', last_seen=?2, updated_at=?2 WHERE username=?1`
  )
    .bind(username, now)
    .run();

  if ((result.meta?.changes ?? 0) === 0) return notFound();

  const peer = await env.DB.prepare(`SELECT username, public_key as publicKey, address, port, device_id as deviceId, status, last_seen as lastSeen FROM peers WHERE username=?1`)
    .bind(username)
    .first();

  return json({ ok: true, peer });
}

async function listPeers(env) {
  const peers = await env.DB.prepare(
    `SELECT username, public_key as publicKey, address, port, device_id as deviceId, status, last_seen as lastSeen
     FROM peers ORDER BY last_seen DESC`
  ).all();

  return json({ peers: peers.results ?? [] });
}

async function enqueueOfflineMessage(env, body) {
  if (!isNonEmptyString(body?.recipient) || !isNonEmptyString(body?.sender) || !isNonEmptyString(body?.payload)) {
    return badRequest("recipient, sender and payload are required");
  }

  const id = crypto.randomUUID();
  const createdAt = new Date().toISOString();
  await env.DB.prepare(
    `INSERT INTO offline_messages (id, sender, recipient, payload, signature, created_at)
     VALUES (?1, ?2, ?3, ?4, ?5, ?6)`
  )
    .bind(id, body.sender.trim().toLowerCase(), body.recipient.trim().toLowerCase(), body.payload, body.signature ?? null, createdAt)
    .run();

  return json({ ok: true, queued: 1, messageId: id });
}

async function getOfflineMessages(env, username, clear) {
  const recipient = username.trim().toLowerCase();
  const result = await env.DB.prepare(
    `SELECT id, sender, recipient, payload, signature, created_at as createdAt
     FROM offline_messages WHERE recipient=?1 AND delivered_at IS NULL ORDER BY created_at ASC`
  )
    .bind(recipient)
    .all();

  const messages = result.results ?? [];

  if (clear && messages.length > 0) {
    const deliveredAt = new Date().toISOString();
    await env.DB.prepare(
      `UPDATE offline_messages SET delivered_at=?2 WHERE recipient=?1 AND delivered_at IS NULL`
    )
      .bind(recipient, deliveredAt)
      .run();
  }

  return json({ recipient, count: messages.length, messages });
}

async function putDeviceKey(env, body) {
  if (!isNonEmptyString(body?.username) || !isNonEmptyString(body?.encryptedPrivateKey)) {
    return badRequest("username and encryptedPrivateKey are required");
  }

  const username = body.username.trim().toLowerCase();
  const updatedAt = new Date().toISOString();

  await env.DB.prepare(
    `INSERT INTO device_keys (username, encrypted_private_key, updated_at)
     VALUES (?1, ?2, ?3)
     ON CONFLICT(username) DO UPDATE SET
      encrypted_private_key=excluded.encrypted_private_key,
      updated_at=excluded.updated_at`
  )
    .bind(username, body.encryptedPrivateKey, updatedAt)
    .run();

  return json({ ok: true });
}

async function getDeviceKey(env, username) {
  const row = await env.DB.prepare(
    `SELECT username, encrypted_private_key as encryptedPrivateKey, updated_at as updatedAt FROM device_keys WHERE username=?1`
  )
    .bind(username.trim().toLowerCase())
    .first();

  if (!row) return notFound();
  return json(row);
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    let response;

    if (request.method === "GET" && url.pathname === "/") {
      response = html(homepage());
    } else if (request.method === "GET" && url.pathname === "/health") {
      response = json({ ok: true, service: "meshtalk-bootstrap-d1" });
    } else if (request.method === "POST" && url.pathname === "/api/register") {
      const body = await readBody(request);
      response = body ? await registerPeer(env, body) : badRequest("Invalid JSON body");
    } else if (request.method === "POST" && url.pathname === "/api/heartbeat") {
      const body = await readBody(request);
      response = body ? await heartbeat(env, body) : badRequest("Invalid JSON body");
    } else if (request.method === "GET" && url.pathname === "/api/peers") {
      response = await listPeers(env);
    } else if (request.method === "POST" && url.pathname === "/api/offline/messages") {
      const body = await readBody(request);
      response = body ? await enqueueOfflineMessage(env, body) : badRequest("Invalid JSON body");
    } else if (request.method === "GET" && url.pathname.startsWith("/api/offline/messages/")) {
      const username = decodeURIComponent(url.pathname.split("/").pop() || "");
      response = isNonEmptyString(username)
        ? await getOfflineMessages(env, username, url.searchParams.get("clear") === "true")
        : badRequest("username is required in path");
    } else if (request.method === "PUT" && url.pathname === "/api/device-key") {
      const body = await readBody(request);
      response = body ? await putDeviceKey(env, body) : badRequest("Invalid JSON body");
    } else if (request.method === "GET" && url.pathname.startsWith("/api/device-key/")) {
      const username = decodeURIComponent(url.pathname.split("/").pop() || "");
      response = isNonEmptyString(username) ? await getDeviceKey(env, username) : badRequest("username is required in path");
    } else {
      response = notFound();
    }

    if (url.pathname !== "/") {
      const actor = request.headers.get("x-meshtalk-user");
      await logAudit(env, request.method, url.pathname, response.status, actor);
    }

    return response;
  },
};
