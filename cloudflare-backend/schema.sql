-- MeshTalk Cloudflare D1 schema
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS peers (
  username TEXT PRIMARY KEY,
  public_key TEXT NOT NULL,
  address TEXT NOT NULL,
  port INTEGER,
  device_id TEXT,
  status TEXT NOT NULL DEFAULT 'online' CHECK (status IN ('online','offline','idle')),
  last_seen TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_peers_status_last_seen ON peers(status, last_seen DESC);

CREATE TABLE IF NOT EXISTS offline_messages (
  id TEXT PRIMARY KEY,
  sender TEXT NOT NULL,
  recipient TEXT NOT NULL,
  payload TEXT NOT NULL,
  signature TEXT,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  delivered_at TEXT,
  FOREIGN KEY (recipient) REFERENCES peers(username) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_offline_recipient_created ON offline_messages(recipient, created_at ASC);

CREATE TABLE IF NOT EXISTS device_keys (
  username TEXT PRIMARY KEY,
  encrypted_private_key TEXT NOT NULL,
  updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE IF NOT EXISTS api_audit_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  method TEXT NOT NULL,
  path TEXT NOT NULL,
  status_code INTEGER NOT NULL,
  actor TEXT,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
