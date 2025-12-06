// Pre-load MPQ files from the server directory into Emscripten virtual filesystem
Module['preRun'] = Module['preRun'] || [];

// Mount IDBFS for persistent save files
Module['preRun'].push(function() {
  console.log('Setting up IDBFS for persistent saves...');

  // SDL uses //libsdl/ as the base path for Emscripten
  // Save files are in //libsdl/diasurgical/devilution/
  // Config files (diablo.ini) would be in //libsdl/diasurgical/
  try {
    // Helper function to create directory if it doesn't exist
    function mkdirSafe(path) {
      try {
        // Check if path exists
        var stat = FS.stat(path);
        // If it exists and is a directory, we're good
        if (FS.isDir(stat.mode)) {
          return;
        }
        // If it exists but is not a directory, this is an error
        console.error('Path exists but is not a directory: ' + path);
        return;
      } catch (e) {
        // Path doesn't exist, try to create it
        try {
          FS.mkdir(path);
        } catch (mkdirErr) {
          // Only throw if it's not an "already exists" error
          if (mkdirErr.errno !== 20 && mkdirErr.errno !== 17) {
            throw mkdirErr;
          }
        }
      }
    }

    // Create SDL directory hierarchy if needed
    mkdirSafe('/libsdl');
    mkdirSafe('/libsdl/diasurgical');

    // Mount the diasurgical directory as IDBFS to persist saves AND settings
    FS.mount(IDBFS, {}, '/libsdl/diasurgical');
    console.log('IDBFS mounted successfully at /libsdl/diasurgical');

    // Sync from IndexedDB to memory (load existing saves)
    Module.addRunDependency('syncfs');
    FS.syncfs(true, function(err) {
      if (err) {
        console.error('Error loading saves from IndexedDB:', err);
      } else {
        console.log('Existing saves loaded from IndexedDB');
      }
      Module.removeRunDependency('syncfs');
    });
  } catch (e) {
    console.error('Error setting up IDBFS:', e);
  }
});

// Load MPQ files from the server directory
Module['preRun'].push(function() {
  // List of MPQ files to try loading (in priority order)
  var mpqFiles = [
    'diabdat.mpq',
    'DIABDAT.MPQ',
    'spawn.mpq',
    'hellfire.mpq',
    'hfmonk.mpq',
    'hfmusic.mpq',
    'hfvoice.mpq',
    'hfbard.mpq',
    'hfbarb.mpq'
  ];

  // Create a promise-based loading system
  var loadPromises = mpqFiles.map(function(filename) {
    return new Promise(function(resolve) {
      fetch(filename)
        .then(function(response) {
          if (response.ok) {
            return response.arrayBuffer();
          }
          throw new Error('File not found');
        })
        .then(function(data) {
          console.log('Loading ' + filename + ' into virtual filesystem...');
          FS.writeFile('/' + filename, new Uint8Array(data));
          console.log('Successfully loaded ' + filename);
          resolve();
        })
        .catch(function() {
          // File doesn't exist, skip silently
          resolve();
        });
    });
  });

  // Wait for all MPQ files to load before continuing
  Module.addRunDependency('loadMPQs');
  Promise.all(loadPromises).then(function() {
    Module.removeRunDependency('loadMPQs');
  });
});

// Track if a sync is in progress to prevent overlapping operations
var syncInProgress = false;

// Expose function to manually save to IndexedDB
Module['saveToIndexedDB'] = function() {
  if (syncInProgress) {
    return;
  }

  syncInProgress = true;
  FS.syncfs(false, function(err) {
    syncInProgress = false;
    if (err) {
      console.error('Error persisting saves to IndexedDB:', err);
    }
  });
};

// Auto-sync to IndexedDB every 30 seconds as a fallback
Module['postRun'] = Module['postRun'] || [];
Module['postRun'].push(function() {
  setInterval(function() {
    if (!syncInProgress) {
      syncInProgress = true;
      FS.syncfs(false, function(err) {
        syncInProgress = false;
        if (err) {
          console.error('Auto-sync error:', err);
        }
      });
    }
  }, 30000);

  // Sync when the page is about to close
  window.addEventListener('beforeunload', function() {
    if (!syncInProgress) {
      FS.syncfs(false, function(err) {
        if (err) console.error('Error syncing on page unload:', err);
      });
    }
  });
});
