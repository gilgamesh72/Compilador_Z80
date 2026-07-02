const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('cminiAPI', {
  // Compilar código fuente
  compile: (sourceCode) => ipcRenderer.invoke('compile', sourceCode),

  // Diálogo para guardar el .tap generado (elige carpeta destino)
  saveTap: (defaultName) => ipcRenderer.invoke('save-tap-dialog', defaultName),

  // Diálogo para abrir un .cmini
  openCmini: () => ipcRenderer.invoke('open-cmini-dialog'),

  // Guardar el .cmini actual
  saveCmini: (sourceCode, currentPath) =>
    ipcRenderer.invoke('save-cmini-dialog', sourceCode, currentPath),

  // Controles de ventana (titlebar personalizado)
  minimize: () => ipcRenderer.send('window-minimize'),
  maximize: () => ipcRenderer.send('window-maximize'),
  close: () => ipcRenderer.send('window-close'),
});
