// File Manager functionality
(function() {
	const modal = document.getElementById('fileManagerModal');
	const fileManagerBtn = document.getElementById('fileManagerBtn');
	const closeModalBtn = document.getElementById('closeModal');
	const dropZone = document.getElementById('dropZone');
	const fileInput = document.getElementById('fileInput');
	const browseBtn = document.getElementById('browseBtn');
	const resetSettingsBtn = document.getElementById('resetSettingsBtn');
	const mpqFilesList = document.getElementById('mpqFilesList');

	// Open/close modal
	fileManagerBtn.addEventListener('click', () => {
		modal.classList.add('show');
		refreshFileList();
	});

	closeModalBtn.addEventListener('click', () => {
		modal.classList.remove('show');
	});

	modal.addEventListener('click', (e) => {
		if (e.target === modal) {
			modal.classList.remove('show');
		}
	});

	// Browse button
	browseBtn.addEventListener('click', () => {
		fileInput.click();
	});

	// Drag and drop
	dropZone.addEventListener('click', () => {
		fileInput.click();
	});

	dropZone.addEventListener('dragover', (e) => {
		e.preventDefault();
		dropZone.classList.add('dragover');
	});

	dropZone.addEventListener('dragleave', () => {
		dropZone.classList.remove('dragover');
	});

	dropZone.addEventListener('drop', (e) => {
		e.preventDefault();
		dropZone.classList.remove('dragover');
		handleFiles(e.dataTransfer.files);
	});

	fileInput.addEventListener('change', (e) => {
		handleFiles(e.target.files);
	});

	// Handle file upload
	function handleFiles(files) {
		if (!files || files.length === 0) return;

		// Wait for Module and FS to be ready
		if (typeof Module === 'undefined' || typeof FS === 'undefined') {
			alert('Game is still loading. Please wait and try again.');
			return;
		}

		const mpqFiles = Array.from(files).filter(f =>
			f.name.toLowerCase().endsWith('.mpq')
		);

		if (mpqFiles.length === 0) {
			alert('Please select MPQ files only.');
			return;
		}

		let processed = 0;
		mpqFiles.forEach(file => {
			const reader = new FileReader();
			reader.onload = function(e) {
				try {
					const data = new Uint8Array(e.target.result);
					// Upload to the devilution subdirectory where the game searches
					const path = '/libsdl/diasurgical/devilution/' + file.name; // Might want to make this dynamic later, since source mods might rename the paths

					// Create directory if it doesn't exist
					try {
						FS.mkdir('/libsdl/diasurgical/devilution');
					} catch (e) {
						// Directory might already exist, ignore
					}

					// Write file to IDBFS-backed directory
					FS.writeFile(path, data);
					console.log('Uploaded:', file.name, '(' + formatBytes(file.size) + ')');

					processed++;
					if (processed === mpqFiles.length) {
						// Sync to IndexedDB
						FS.syncfs(false, function(err) {
							if (err) {
								console.error('Error syncing files:', err);
								alert('Error saving files. Check console.');
							} else {
								alert('Files uploaded successfully! Reloading game...');
								setTimeout(() => location.reload(), 500);
							}
						});
					}
				} catch (err) {
					console.error('Error writing file:', err);
					alert('Error uploading file: ' + file.name);
				}
			};
			reader.readAsArrayBuffer(file);
		});
	}

	// Refresh file list
	function refreshFileList() {
		if (typeof Module === 'undefined' || typeof FS === 'undefined') {
			mpqFilesList.innerHTML = '<p class="info-text">Game is loading...</p>';
			return;
		}

		try {
			// Check if devilution directory exists
			try {
				FS.stat('/libsdl/diasurgical/devilution');
			} catch (e) {
				// Directory doesn't exist yet
				mpqFilesList.innerHTML = '<p class="info-text">No MPQ files found.</p>';
				return;
			}

			const files = FS.readdir('/libsdl/diasurgical/devilution');
			const mpqFiles = files.filter(f =>
				f.toLowerCase().endsWith('.mpq') && f !== '.' && f !== '..'
			);

			if (mpqFiles.length === 0) {
				mpqFilesList.innerHTML = '<p class="info-text">No MPQ files found.</p>';
				return;
			}

			mpqFilesList.innerHTML = mpqFiles.map(filename => {
				const path = '/libsdl/diasurgical/devilution/' + filename;
				const stat = FS.stat(path);
				return `
					<div class="file-item">
						<span class="file-item-name">${filename}</span>
						<span class="file-item-size">${formatBytes(stat.size)}</span>
						<button class="btn btn-delete" onclick="deleteFile('${filename}')">Delete</button>
					</div>
				`;
			}).join('');
		} catch (err) {
			console.error('Error reading files:', err);
			mpqFilesList.innerHTML = '<p class="info-text">Error reading files.</p>';
		}
	}

	// Delete file
	window.deleteFile = function(filename) {
		if (!confirm('Delete ' + filename + '? This will reload the game.')) {
			return;
		}

		try {
			const path = '/libsdl/diasurgical/devilution/' + filename;
			FS.unlink(path);

			// Sync deletion to IndexedDB
			FS.syncfs(false, function(err) {
				if (err) {
					console.error('Error syncing deletion:', err);
					alert('Error deleting file. Check console.');
				} else {
					alert('File deleted! Reloading game...');
					setTimeout(() => location.reload(), 500);
				}
			});
		} catch (err) {
			console.error('Error deleting file:', err);
			alert('Error deleting file: ' + filename);
		}
	};

	// Reset settings
	resetSettingsBtn.addEventListener('click', () => {
		if (!confirm('Reset game settings? This will delete diablo.ini but keep your saves. The game will reload.')) {
			return;
		}

		try {
			const iniPath = '/libsdl/diasurgical/devilution/diablo.ini';

			// Check if file exists
			try {
				FS.stat(iniPath);
				// File exists, delete it
				FS.unlink(iniPath);
				console.log('Deleted diablo.ini');
			} catch (e) {
				// File doesn't exist, that's fine
				console.log('diablo.ini not found (already reset)');
			}

			// Sync to IndexedDB
			FS.syncfs(false, function(err) {
				if (err) {
					console.error('Error syncing settings reset:', err);
					alert('Error resetting settings. Check console.');
				} else {
					alert('Settings reset! Reloading game...');
					setTimeout(() => location.reload(), 500);
				}
			});
		} catch (err) {
			console.error('Error resetting settings:', err);
			alert('Error resetting settings.');
		}
	});

	// Helper function
	function formatBytes(bytes) {
		if (bytes === 0) return '0 Bytes';
		const k = 1024;
		const sizes = ['Bytes', 'KB', 'MB', 'GB'];
		const i = Math.floor(Math.log(bytes) / Math.log(k));
		return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
	}
})();
