# MeshTalk Cloudflare Bootstrap Backend (D1)

This Worker powers MeshTalk v2 bootstrap services using Cloudflare **Workers + D1**.

## What is included

- API homepage UI at `/`
- Peer registration/discovery
- Presence heartbeat updates
- Offline message queue retrieval + clear-on-sync
- Optional multi-device encrypted key storage
- API request audit log table

## API endpoints

- `GET /`
- `GET /health`
- `POST /api/register`
- `POST /api/heartbeat`
- `GET /api/peers`
- `POST /api/offline/messages`
- `GET /api/offline/messages/:username?clear=true`
- `PUT /api/device-key`
- `GET /api/device-key/:username`

## Required tooling

- Node.js 18+
- npm
- Cloudflare account with Workers + D1 enabled

## Setup

```bash
cd cloudflare-backend
npm install
```

### Create D1 database and apply schema

```bash
npx wrangler d1 create meshtalk-bootstrap-db
# copy database_id into wrangler.toml
npx wrangler d1 execute meshtalk-bootstrap-db --file=./schema.sql
```

## Local development

```bash
npm run dev
```

## Deploy

```bash
npm run deploy
```

## Notes on requested UI parity

The target reference URL was unreachable from this environment (`403 CONNECT tunnel failed`), so exact pixel-level matching could not be validated here. The Worker homepage was updated to a polished dark dashboard layout and can be iterated once the reference page is accessible.
