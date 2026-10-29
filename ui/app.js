const names = {
  overview: 'Overview', backup: 'Create backup', history: 'Version history', verify: 'Verify integrity',
  restore: 'Restore version', recover: 'Auto recover', undo: 'Undo latest change', corrupt: 'Simulate corruption',
  checksum: 'Checksums', bench: 'Benchmarks', help: 'CLI help',
};
const navButtons = [...document.querySelectorAll('.nav-item')];
const views = [...document.querySelectorAll('.view')];
const resultDock = document.querySelector('#result-dock');
let lastCommand = '';

function showView(name) {
  views.forEach(view => view.classList.toggle('active', view.id === `view-${name}`));
  navButtons.forEach(button => button.classList.toggle('active', button.dataset.view === name));
  document.querySelector('#crumb-current').textContent = names[name] || 'Overview';
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

navButtons.forEach(button => button.addEventListener('click', () => showView(button.dataset.view)));
document.querySelectorAll('[data-go]').forEach(button => button.addEventListener('click', () => showView(button.dataset.go)));

function quoteArg(value) {
  const text = String(value);
  return /[\s"']/u.test(text) ? `"${text.replaceAll('"', '\\"')}"` : text;
}

function formValues(form, commandOverride) {
  const data = Object.fromEntries(new FormData(form).entries());
  data.command = commandOverride || form.dataset.command;
  return data;
}

async function runOperation(data) {
  const resultOutput = document.querySelector('#result-output');
  const resultTitle = document.querySelector('#result-title');
  const resultCommand = document.querySelector('#result-command');
  resultDock.hidden = false;
  resultDock.dataset.ok = '';
  resultTitle.textContent = 'Running operation…';
  resultOutput.textContent = 'RecoverX is working…';
  resultDock.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  try {
    const response = await fetch('/api/run', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data),
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'The request could not be run.');
    const store = data.store || '.recoverx/ui-demo-store';
    const args = data.command === 'backup'
      ? ['backup', data.file, store, ...(data.chunkBytes ? [data.chunkBytes] : [])]
      : data.command === 'verify'
        ? (data.verifyFile ? ['verify', data.verifyFile, data.versionId, store] : ['verify', data.versionId, store])
        : data.command === 'restore'
          ? ['restore', data.versionId, data.outputFile, store]
          : data.command === 'history' || data.command === 'list' || data.command === 'recover' || data.command === 'undo'
            ? [data.command, data.file, store]
            : data.command === 'corrupt'
              ? ['corrupt', data.file, data.versionId, data.mode, store]
              : data.command === 'bench'
                ? ['bench', ...(data.outputDir || data.trials ? [data.outputDir || '.recoverx/ui-demo-bench'] : []), ...(data.trials ? [data.trials] : [])]
                : [data.command];
    lastCommand = `.\\build\\recoverx.exe ${args.map(quoteArg).join(' ')}`;
    resultCommand.textContent = lastCommand;
    resultOutput.textContent = result.output || result.error;
    resultTitle.textContent = result.ok ? 'Operation completed' : `Operation returned code ${result.code}`;
    resultDock.dataset.ok = String(result.ok);
  } catch (error) {
    lastCommand = '';
    resultCommand.textContent = '';
    resultTitle.textContent = 'Could not run operation';
    resultOutput.textContent = error.message;
    resultDock.dataset.ok = 'false';
  }
}

document.querySelectorAll('form[data-command]').forEach(form => form.addEventListener('submit', event => {
  event.preventDefault();
  if (!form.reportValidity()) return;
  runOperation(formValues(form));
}));

document.querySelector('[data-run-alias="list"]').addEventListener('click', () => {
  const form = document.querySelector('form[data-command="history"]');
  if (!form.reportValidity()) return;
  runOperation(formValues(form, 'list'));
});

document.querySelector('#dismiss-result').addEventListener('click', () => { resultDock.hidden = true; });
document.querySelector('#copy-command').addEventListener('click', async event => {
  if (!lastCommand) return;
  await navigator.clipboard.writeText(lastCommand);
  event.currentTarget.textContent = 'Copied';
  setTimeout(() => { event.currentTarget.textContent = 'Copy'; }, 1200);
});

fetch('/api/status').then(response => response.json()).then(status => {
  document.querySelector('#server-status').textContent = status.ready ? 'RecoverX engine ready' : 'Build is required';
  document.querySelector('#engine-path').textContent = status.executable;
  document.querySelector('.status-dot').style.background = status.ready ? '#36b681' : '#ed9f36';
}).catch(() => {
  document.querySelector('#server-status').textContent = 'Local engine unavailable';
});
