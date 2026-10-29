const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const projectRoot = path.resolve(__dirname, '..');
const port = Number(process.env.RECOVERX_UI_PORT || 8765);
const executable = path.join(projectRoot, 'build', 'recoverx.exe');
const assets = {
  '/': ['index.html', 'text/html; charset=utf-8'],
  '/index.html': ['index.html', 'text/html; charset=utf-8'],
  '/styles.css': ['styles.css', 'text/css; charset=utf-8'],
  '/app.js': ['app.js', 'text/javascript; charset=utf-8'],
};

function send(res, status, body, type = 'application/json; charset=utf-8') {
  res.writeHead(status, { 'Content-Type': type, 'Cache-Control': 'no-store' });
  res.end(type.startsWith('application/json') ? JSON.stringify(body) : body);
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => {
      body += chunk;
      if (body.length > 16_384) reject(new Error('Request is too large.'));
    });
    req.on('end', () => {
      try { resolve(JSON.parse(body || '{}')); }
      catch { reject(new Error('Request must contain valid JSON.')); }
    });
    req.on('error', reject);
  });
}

function required(value, label) {
  const text = String(value || '').trim();
  if (!text) throw new Error(`${label} is required.`);
  return text;
}

function buildArgs(data) {
  const store = String(data.store || '.recoverx/ui-demo-store').trim();
  const file = () => required(data.file, 'Source file');
  const id = () => {
    const value = required(data.versionId, 'Version ID');
    if (!/^\d+$/.test(value) || Number(value) < 1) throw new Error('Version ID must be a positive integer.');
    return value;
  };
  switch (data.command) {
    case 'backup': {
      const args = ['backup', file(), store];
      const chunk = String(data.chunkBytes || '').trim();
      if (chunk) {
        if (!/^\d+$/.test(chunk) || Number(chunk) < 1) throw new Error('Chunk size must be a positive integer.');
        args.push(chunk);
      }
      return args;
    }
    case 'history':
    case 'list': return [data.command, file(), store];
    case 'verify': {
      const source = String(data.verifyFile || '').trim();
      return source ? ['verify', source, id(), store] : ['verify', id(), store];
    }
    case 'restore': return ['restore', id(), required(data.outputFile, 'Output file'), store];
    case 'recover':
    case 'undo': return [data.command, file(), store];
    case 'corrupt': {
      const mode = required(data.mode, 'Corruption mode');
      if (!['bitflip', 'burst', 'swap', 'truncate', 'zero', 'deletechunk'].includes(mode)) {
        throw new Error('Choose one of the supported corruption modes.');
      }
      return ['corrupt', file(), id(), mode, store];
    }
    case 'checksum': return ['checksum'];
    case 'bench': {
      const args = ['bench'];
      const output = String(data.outputDir || '').trim();
      const trials = String(data.trials || '').trim();
      if (output || trials) args.push(output || '.recoverx/ui-demo-bench');
      if (trials) {
        if (!/^\d+$/.test(trials) || Number(trials) < 1) throw new Error('Trial count must be a positive integer.');
        args.push(trials);
      }
      return args;
    }
    case 'help': return ['help'];
    default: throw new Error('Choose a RecoverX operation.');
  }
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || '127.0.0.1'}`);
  if (req.method === 'GET' && assets[url.pathname]) {
    const [name, type] = assets[url.pathname];
    fs.readFile(path.join(__dirname, name), (error, content) => {
      if (error) return send(res, 404, 'UI asset not found.', 'text/plain; charset=utf-8');
      send(res, 200, content, type);
    });
    return;
  }
  if (req.method === 'GET' && url.pathname === '/api/status') {
    return send(res, 200, {
      ready: fs.existsSync(executable),
      executable: path.relative(projectRoot, executable),
      store: '.recoverx/ui-demo-store',
    });
  }
  if (req.method === 'POST' && url.pathname === '/api/run') {
    try {
      const data = await readJson(req);
      const args = buildArgs(data);
      if (!fs.existsSync(executable)) throw new Error('RecoverX is not built yet. Run the CMake build command from the project folder.');
      const child = spawn(executable, args, { cwd: projectRoot, windowsHide: true });
      let output = '';
      let settled = false;
      const timeoutMs = args[0] === 'bench' ? 10 * 60 * 1000 : 60 * 1000;
      const timer = setTimeout(() => child.kill(), timeoutMs);
      child.stdout.on('data', chunk => { output += chunk.toString(); });
      child.stderr.on('data', chunk => { output += chunk.toString(); });
      child.on('error', error => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        send(res, 500, { ok: false, error: error.message });
      });
      child.on('close', code => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        send(res, 200, { ok: code === 0, code, output: output.trim() || '(No output)' });
      });
    } catch (error) {
      send(res, 400, { ok: false, error: error.message });
    }
    return;
  }
  send(res, 404, { error: 'Not found' });
});

server.listen(port, '127.0.0.1', () => {
  console.log(`RecoverX UI is ready at http://127.0.0.1:${port}`);
  console.log('Keep this terminal open while presenting. Press Ctrl+C to stop.');
});
