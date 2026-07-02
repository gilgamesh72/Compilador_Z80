/* ══════════════════════════════════════════════════════════
   C-Mini IDE — renderer.js
   Lógica del editor, compilación y terminal
   ══════════════════════════════════════════════════════════ */

// ── Estado global ────────────────────────────────────────
let currentFilePath = null;
let isDirty = false;
let lastTapReady = false;   // true si el .tap fue generado exitosamente
let isCompiling = false;
let editorHeightPct = 65; // % de la pantalla para el editor

// ── Inicializar CodeMirror ───────────────────────────────
const cmEditor = CodeMirror.fromTextArea(document.getElementById('code-editor'), {
  mode: 'text/x-csrc',
  theme: 'one-dark',
  lineNumbers: true,
  matchBrackets: true,
  autoCloseBrackets: true,
  styleActiveLine: true,
  indentUnit: 4,
  tabSize: 4,
  indentWithTabs: false,
  lineWrapping: false,
  extraKeys: {
    'F5': () => compile(),
    'Ctrl-S': () => saveFile(),
    'Ctrl-O': () => openFile(),
    'Ctrl-N': () => newFile(),
    'Tab': (cm) => cm.execCommand('indentMore'),
    'Shift-Tab': (cm) => cm.execCommand('indentLess'),
  },
});

// Código de ejemplo C-Mini para el editor
cmEditor.setValue(`// Programa de ejemplo en C-Mini
int suma(int a, int b) {
    return a + b;
}

int main() {
    int x;
    int y;
    x = 10;
    y = 20;
    int resultado;
    resultado = suma(x, y);
    return resultado;
}
`);
cmEditor.clearHistory();

// ── Posición del cursor ──────────────────────────────────
const cursorPosEl = document.getElementById('cursor-pos');
cmEditor.on('cursorActivity', () => {
  const cur = cmEditor.getCursor();
  cursorPosEl.textContent = `Línea ${cur.line + 1}, Col ${cur.ch + 1}`;
});

// ── Dirty tracking ───────────────────────────────────────
const dirtyIndicator = document.getElementById('dirty-indicator');
cmEditor.on('change', () => {
  if (!isDirty) {
    isDirty = true;
    dirtyIndicator.style.display = 'inline';
  }
});

// ── Refs DOM ─────────────────────────────────────────────
const terminalEl    = document.getElementById('terminal-output');
const statusPill    = document.getElementById('status-pill');
const statusText    = document.getElementById('status-text');
const compileIcon   = document.getElementById('compile-icon');
const compileLabel  = document.getElementById('compile-label');
const btnCompile    = document.getElementById('btn-compile');
const btnSaveTap    = document.getElementById('btn-save-tap');
const fileBadge     = document.getElementById('file-badge');

// ══════════════════════════════════════════════════════════
// TERMINAL HELPERS
// ══════════════════════════════════════════════════════════

function clearTerminal() {
  terminalEl.innerHTML = '';
}

function termLine(text, cls = 'term-info', prompt = '>') {
  const wrap = document.createElement('div');
  wrap.className = 'term-line';

  const p = document.createElement('span');
  p.className = 'term-prompt';
  p.textContent = prompt;

  const t = document.createElement('span');
  t.className = cls;
  t.textContent = text;

  wrap.appendChild(p);
  wrap.appendChild(t);
  terminalEl.appendChild(wrap);
  terminalEl.scrollTop = terminalEl.scrollHeight;
}

function termSeparator() {
  const hr = document.createElement('div');
  hr.className = 'term-separator';
  terminalEl.appendChild(hr);
}

function termBanner(text, type) {
  const banner = document.createElement('div');
  banner.className = `term-banner ${type}`;
  banner.textContent = type === 'success' ? `${text}` : `${text}`;
  terminalEl.appendChild(banner);
  terminalEl.scrollTop = terminalEl.scrollHeight;
}

function termTimestamp() {
  const now = new Date();
  const ts = now.toLocaleTimeString('es-MX', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  termLine(`[${ts}]`, 'term-dim', ' ');
}

// ══════════════════════════════════════════════════════════
// STATUS HELPERS
// ══════════════════════════════════════════════════════════

function setStatus(state, text) {
  statusPill.className = `status-pill ${state}`;
  statusText.textContent = text;
}

function setCompiling(active) {
  isCompiling = active;
  btnCompile.disabled = active;
  if (active) {
    compileIcon.textContent = '↻';
    compileIcon.classList.add('spinning');
    compileLabel.textContent = 'Compilando…';
    setStatus('running', 'Compilando');
  } else {
    compileIcon.textContent = '▶';
    compileIcon.classList.remove('spinning');
    compileLabel.textContent = 'Compilar';
  }
}

// ══════════════════════════════════════════════════════════
// COMPILAR
// ══════════════════════════════════════════════════════════

async function compile() {
  if (isCompiling) return;

  const sourceCode = cmEditor.getValue();
  if (!sourceCode.trim()) {
    termLine('El editor está vacío. Escribe código C-Mini primero.', 'term-warn');
    return;
  }

  clearTerminal();
  setCompiling(true);
  btnSaveTap.disabled = true;
  lastTapReady = false;

  termLine('Iniciando compilación', 'term-dim', '─');
  termTimestamp();
  termSeparator();

  try {
    const result = await window.cminiAPI.compile(sourceCode);

    // Mostrar output del compilador línea a línea
    if (result.output) {
      const lines = result.output.split('\n');
      for (const line of lines) {
        if (!line.trim()) continue;

        let cls = 'term-info';
        if (/\[1\/4\]|\[2\/4\]|\[3\/4\]|\[4\/4\]/.test(line)) cls = 'term-phase';
        else if (/error|Error/i.test(line)) cls = 'term-error';
        else if (/warn|advertencia/i.test(line)) cls = 'term-warn';
        else if (/éxito|exitosa|finaliz/i.test(line)) cls = 'term-success';

        termLine(line, cls);
      }
    }

    termSeparator();

    if (result.success) {
      // El .asm se guardó automáticamente en compilador/output.asm
      termLine('output.asm guardado en: compilador/output.asm', 'term-dim', '→');

      // Estado del TAP
      if (result.tapContent) {
        termLine('output.tap generado correctamente', 'term-phase', '→');
        lastTapReady = true;
        btnSaveTap.disabled = false;
        termBanner('Compilación exitosa — haz clic en “Guardar TAP” para elegir dónde guardarlo', 'success');
      } else {
        if (result.tapLog) termLine(result.tapLog, 'term-warn', '!');
        termBanner('Compilación exitosa — ASM generado (TAP no disponible)', 'success');
      }
      setStatus('success', 'Éxito');
    } else {
      termBanner('Compilación fallida — revisa los errores arriba', 'error');
      setStatus('error', 'Error');
    }
  } catch (err) {
    termSeparator();
    termLine(`Error inesperado: ${err.message}`, 'term-error');
    termBanner('Fallo al ejecutar el compilador', 'error');
    setStatus('error', 'Error');
  } finally {
    setCompiling(false);
    termTimestamp();
  }
}

// ══════════════════════════════════════════════════════════
// ARCHIVO: NUEVO / ABRIR / GUARDAR
// ══════════════════════════════════════════════════════════

function newFile() {
  if (isDirty) {
    // Pequeña confirmación visual en la terminal en lugar de alert
    clearTerminal();
    termLine('Nuevo archivo — los cambios no guardados se perderán.', 'term-warn');
  }
  cmEditor.setValue('');
  currentFilePath = null;
  isDirty = false;
  dirtyIndicator.style.display = 'none';
  fileBadge.textContent = 'Sin título.cmini';
  lastTapReady = false;
  btnSaveTap.disabled = true;
  setStatus('idle', 'Listo');
  clearTerminal();
  terminalEl.innerHTML = `
    <div class="term-welcome">
      <div class="term-logo">⚙  C-Mini → Z80 Compiler</div>
      <div class="term-subtitle">Escribe tu código y presiona <kbd>F5</kbd> o haz clic en <strong>Compilar</strong></div>
    </div>`;
  cmEditor.focus();
}

async function openFile() {
  const result = await window.cminiAPI.openCmini();
  if (!result.opened) return;

  cmEditor.setValue(result.content);
  cmEditor.clearHistory();
  currentFilePath = result.filePath;
  isDirty = false;
  dirtyIndicator.style.display = 'none';

  const fileName = result.filePath.split(/[\\/]/).pop();
  fileBadge.textContent = fileName;

  clearTerminal();
  termLine(`Archivo abierto: ${result.filePath}`, 'term-info');
  setStatus('idle', 'Listo');
  cmEditor.focus();
}

async function saveFile() {
  const sourceCode = cmEditor.getValue();
  const result = await window.cminiAPI.saveCmini(sourceCode, currentFilePath);

  if (result.saved) {
    currentFilePath = result.filePath;
    isDirty = false;
    dirtyIndicator.style.display = 'none';
    const fileName = result.filePath.split(/[\\/]/).pop();
    fileBadge.textContent = fileName;
    termLine(`Guardado: ${result.filePath}`, 'term-success', '');
  } else if (result.error) {
    termLine(`Error al guardar: ${result.error}`, 'term-error');
  }
}

// ══════════════════════════════════════════════════════════
// BOTONES
// ══════════════════════════════════════════════════════════

document.getElementById('btn-compile').addEventListener('click', compile);
document.getElementById('btn-new').addEventListener('click', newFile);
document.getElementById('btn-open').addEventListener('click', openFile);
document.getElementById('btn-save').addEventListener('click', saveFile);
document.getElementById('btn-clear').addEventListener('click', () => {
  clearTerminal();
  setStatus('idle', 'Listo');
});

document.getElementById('btn-save-tap').addEventListener('click', async () => {
  if (!lastTapReady) return;
  const result = await window.cminiAPI.saveTap('programa.tap');
  if (result.saved) {
    termLine(`TAP guardado en: ${result.filePath}`, 'term-success', '');
  } else if (result.error) {
    termLine(`Error al guardar TAP: ${result.error}`, 'term-error');
  }
});

// ── Titlebar controls ────────────────────────────────────
document.getElementById('btn-minimize').addEventListener('click', () => window.cminiAPI.minimize());
document.getElementById('btn-maximize').addEventListener('click', () => window.cminiAPI.maximize());
document.getElementById('btn-close').addEventListener('click', () => window.cminiAPI.close());

// ══════════════════════════════════════════════════════════
// RESIZE PANEL (drag handle)
// ══════════════════════════════════════════════════════════

const resizeHandle = document.getElementById('resize-handle');
const editorPanel  = document.getElementById('editor-panel');
const outputPanel  = document.getElementById('output-panel');
const mainLayout   = document.getElementById('main-layout');

let resizing = false;
let startY   = 0;
let startEditorH = 0;

resizeHandle.addEventListener('mousedown', (e) => {
  resizing = true;
  startY = e.clientY;
  startEditorH = editorPanel.getBoundingClientRect().height;
  resizeHandle.classList.add('dragging');
  document.body.style.cursor = 'ns-resize';
  e.preventDefault();
});

document.addEventListener('mousemove', (e) => {
  if (!resizing) return;
  const totalH = mainLayout.getBoundingClientRect().height;
  const delta = e.clientY - startY;
  const newEditorH = Math.min(Math.max(startEditorH + delta, 100), totalH - 100);
  const newOutputH = totalH - newEditorH - 5; // 5px handle
  editorPanel.style.flex = 'none';
  editorPanel.style.height = newEditorH + 'px';
  outputPanel.style.flex = 'none';
  outputPanel.style.height = newOutputH + 'px';
  cmEditor.refresh();
});

document.addEventListener('mouseup', () => {
  if (!resizing) return;
  resizing = false;
  resizeHandle.classList.remove('dragging');
  document.body.style.cursor = '';
  cmEditor.refresh();
});

// ── Focus editor on load ─────────────────────────────────
window.addEventListener('load', () => {
  cmEditor.refresh();
  cmEditor.focus();
});
