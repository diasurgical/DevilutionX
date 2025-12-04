// Pre-load MPQ files from the server directory into Emscripten virtual filesystem
Module['preRun'] = Module['preRun'] || [];
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
