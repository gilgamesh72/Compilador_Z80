const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const { exec, execFile } = require('child_process');
const fs = require('fs');
const os = require('os');

// Ruta al compilador dentro del proyecto (lado Linux/WSL)
// Ajusta esta ruta según donde esté tu proyecto en WSL
const PROJECT_DIR = path.join(__dirname, '..');
const COMPILER_BIN = path.join(PROJECT_DIR, 'bin', 'c-mini-compiler');

function toWslPath(winPath) {
  // Convierte C:\Users\... → /mnt/c/Users/...
  return winPath
    .replace(/\\/g, '/')
    .replace(/^([A-Za-z]):/, (_, letter) => `/mnt/${letter.toLowerCase()}`);
}

function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    minWidth: 900,
    minHeight: 600,
    title: 'C-Mini IDE',
    backgroundColor: '#1e1e2e',
    frame: false, // Frameless para custom titlebar
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  win.loadFile('index.html');

  // Ctrl+Shift+I → DevTools (inspector de CSS/JS en vivo)
  // Ctrl+R       → Recarga la UI sin reiniciar la app
  const { globalShortcut } = require('electron');
  win.webContents.on('before-input-event', (event, input) => {
    if (input.control && input.shift && input.key === 'I') {
      win.webContents.toggleDevTools();
    }
    if (input.control && input.key === 'r') {
      win.webContents.reload();
    }
  });

  // Eventos de ventana para el titlebar personalizado
  ipcMain.on('window-minimize', () => win.minimize());
  ipcMain.on('window-maximize', () => {
    if (win.isMaximized()) win.unmaximize();
    else win.maximize();
  });
  ipcMain.on('window-close', () => win.close());
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

// ── IPC: Compilar ────────────────────────────────────────────
ipcMain.handle('compile', async (event, sourceCode) => {
  return new Promise((resolve) => {
    // 1. Guardar el código fuente en un archivo temporal dentro del proyecto
    const tmpFile = path.join(PROJECT_DIR, `_cmini_temp_${Date.now()}.cmini`);
    const tmpFileWsl   = toWslPath(tmpFile);
    const compilerWsl  = toWslPath(COMPILER_BIN);
    const projectDirWsl = toWslPath(PROJECT_DIR);
    const asmPathWin   = path.join(PROJECT_DIR, 'output.asm');
    const tapPathWin   = path.join(PROJECT_DIR, 'output.tap');

    try {
      fs.writeFileSync(tmpFile, sourceCode, 'utf8');
    } catch (err) {
      return resolve({ success: false, output: `Error al crear archivo temporal:\n${err.message}` });
    }

    // 2. Ejecutar el compilador via WSL
    //    Hacemos 'cd' al directorio del proyecto para que output.asm se genere ahí
    const command = `wsl bash -c "cd '${projectDirWsl}' && '${compilerWsl}' '${tmpFileWsl}'"`;

    exec(command, { timeout: 30000, cwd: PROJECT_DIR }, (error, stdout, stderr) => {
      // Limpiar archivo temporal
      try { fs.unlinkSync(tmpFile); } catch (_) {}

      const output = [stdout, stderr].filter(Boolean).join('\n').trim();
      const hasFailed = (error && error.code && error.code !== 0)
        || /Error durante|errores semánticos/i.test(output);

      if (hasFailed) {
        return resolve({ success: false, output: output || (error ? error.message : 'Error desconocido') });
      }

      // 3. Verificar que output.asm fue generado
      if (!fs.existsSync(asmPathWin)) {
        return resolve({ success: false, output: output + '\n[Error: no se generó output.asm]' });
      }

      // 4. Generar output.tap con pasmo usando --tapbas
      //    --tapbas genera un loader BASIC que auto-ejecuta el programa en el emulador
      const tapCmd = `wsl bash -c "cd '${projectDirWsl}' && pasmo --tapbas output.asm output.tap 2>&1"`;

      exec(tapCmd, { timeout: 15000, cwd: PROJECT_DIR }, (tapErr, tapStdout, tapStderr) => {
        const tapOutput = [tapStdout, tapStderr].filter(Boolean).join('\n').trim();
        const tapFailed = (tapErr && tapErr.code && tapErr.code !== 0)
          || /not found|command not found/i.test(tapOutput);

        let tapContent = null;
        let tapLog = '';

        if (tapFailed) {
          // pasmo no está instalado o falló
          tapLog = tapOutput.includes('not found')
            ? '[TAP] pasmo no está instalado en WSL. Instala con: sudo apt install pasmo'
            : `[TAP] Error al generar TAP:\n${tapOutput}`;
        } else {
          // Leer el .tap generado
          try {
            tapContent = fs.readFileSync(tapPathWin);
          } catch (_) {
            tapLog = '[TAP] El archivo output.tap no se pudo leer.';
          }
        }

        resolve({
          success: true,
          output,
          tapContent,     // Buffer con contenido del .tap (null si falló)
          tapLog,         // Mensaje de error/info sobre el TAP
        });
      });
    });
  });
});


// ── IPC: Diálogo "Guardar TAP" ─────────────────────────────────────────
ipcMain.handle('save-tap-dialog', async (event, defaultName) => {
  const { canceled, filePath } = await dialog.showSaveDialog({
    title: 'Guardar archivo TAP (ZX Spectrum)',
    defaultPath: path.join(app.getPath('documents'), defaultName || 'programa.tap'),
    filters: [
      { name: 'Archivos TAP (ZX Spectrum)', extensions: ['tap'] },
      { name: 'Todos los archivos', extensions: ['*'] },
    ],
  });

  if (canceled || !filePath) return { saved: false };

  // Copiar output.tap al destino elegido y borrar el original del proyecto
  const sourceTap = path.join(PROJECT_DIR, 'output.tap');
  try {
    fs.copyFileSync(sourceTap, filePath);
    // Borrar el .tap temporal de la carpeta del proyecto (evita doble copia)
    try { fs.unlinkSync(sourceTap); } catch (_) {}
    return { saved: true, filePath };
  } catch (err) {
    return { saved: false, error: err.message };
  }
});

// ── IPC: Diálogo "Abrir .cmini" ────────────────────────────────────────────
ipcMain.handle('open-cmini-dialog', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog({
    title: 'Abrir archivo C-Mini',
    filters: [
      { name: 'Archivos C-Mini', extensions: ['cmini', 'c', 'txt'] },
      { name: 'Todos los archivos', extensions: ['*'] },
    ],
    properties: ['openFile'],
  });

  if (canceled || filePaths.length === 0) return { opened: false };

  try {
    const content = fs.readFileSync(filePaths[0], 'utf8');
    return { opened: true, content, filePath: filePaths[0] };
  } catch (err) {
    return { opened: false, error: err.message };
  }
});

// ── IPC: Guardar archivo .cmini ────────────────────────────────────────────
ipcMain.handle('save-cmini-dialog', async (event, sourceCode, currentPath) => {
  let savePath = currentPath;

  if (!savePath) {
    const { canceled, filePath } = await dialog.showSaveDialog({
      title: 'Guardar archivo C-Mini',
      defaultPath: path.join(app.getPath('documents'), 'programa.cmini'),
      filters: [
        { name: 'Archivos C-Mini', extensions: ['cmini'] },
        { name: 'Todos los archivos', extensions: ['*'] },
      ],
    });
    if (canceled || !filePath) return { saved: false };
    savePath = filePath;
  }

  try {
    fs.writeFileSync(savePath, sourceCode, 'utf8');
    return { saved: true, filePath: savePath };
  } catch (err) {
    return { saved: false, error: err.message };
  }
});
