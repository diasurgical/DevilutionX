package org.diasurgical.devilutionx;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.Nullable;
import android.os.Bundle;
import android.support.annotation.RequiresApi;
import android.support.v4.provider.DocumentFile;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Objects;

public class ImportActivity extends Activity {

	private static final int IMPORT_REQUEST_CODE = 0xD1AB70;
	private boolean userConfirmed = false;

	@RequiresApi(api = Build.VERSION_CODES.KITKAT)
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		ExternalFilesManager fileManager = new ExternalFilesManager(this);
		String externalFilesDir =  fileManager.getExternalFilesDirectory();

		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(getString(R.string.import_data_info, externalFilesDir));
		builder.setPositiveButton(R.string.ok_button, (dialog, which) -> {
			this.userConfirmed = true;
			Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
			intent.addCategory(Intent.CATEGORY_OPENABLE);
			intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
			intent.setType("*/*");
			startActivityForResult(intent, IMPORT_REQUEST_CODE);
		});
		builder.setOnDismissListener(dialog -> {
			if (!this.userConfirmed && !isFinishing()) finish();
		});

		AlertDialog dialog = builder.create();
		dialog.show();
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
		if (requestCode != IMPORT_REQUEST_CODE)
			return;

		if (resultCode == Activity.RESULT_OK && data != null) {
			ArrayList<Uri> uriList = getItemUris(data.getClipData());

			Uri dataUri = data.getData();
			if (dataUri != null)
				uriList.add(dataUri);

			ArrayList<String> fileNames = getFileNames(uriList);
			ArrayList<String> overwrittenFiles = getOverwrittenFiles(fileNames);
			if (overwrittenFiles.isEmpty()) {
				importFiles(uriList);
				finish();
				return;
			}

			String overwrittenFileList = String.join("\n", overwrittenFiles);
			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setMessage(getString(R.string.overwrite_query, overwrittenFileList));
			builder.setPositiveButton(R.string.continue_button, (dialog, which) -> { importFiles(uriList); });
			builder.setNegativeButton(R.string.cancel_button, null);
			builder.setOnDismissListener(dialog -> finish());

			AlertDialog dialog = builder.create();
			dialog.show();
		} else {
			super.onBackPressed();
		}
	}

	private ArrayList<Uri> getItemUris(ClipData clipData) {
		ArrayList<Uri> uriList = new ArrayList<>();
		if (clipData == null)
			return uriList;

		for (int i = 0; i < clipData.getItemCount(); i++) {
			ClipData.Item item = clipData.getItemAt(i);
			if (item == null)
				continue;

			Uri itemUri = item.getUri();
			if (itemUri == null)
				continue;

			uriList.add(itemUri);
		}

		return uriList;
	}

	private ArrayList<String> getFileNames(ArrayList<Uri> uriList) {
		ArrayList<String> fileNames = new ArrayList<>();
		for (Uri uri : uriList) {
			DocumentFile file = DocumentFile.fromSingleUri(getApplicationContext(), uri);
			if (file == null)
				continue;

			String fileName = file.getName();
			fileNames.add(fileName);
		}
		return fileNames;
	}

	private ArrayList<String> getOverwrittenFiles(ArrayList<String> fileNames) {
		ArrayList<String> overwrittenFiles = new ArrayList<>();
		ExternalFilesManager fileManager = new ExternalFilesManager(this);
		for (String fileName : fileNames) {
			if (fileManager.hasFile(fileName))
				overwrittenFiles.add(fileName);
		}
		return overwrittenFiles;
	}

	private void importFiles(ArrayList<Uri> uriList) {
		for (Uri uri : uriList)
			importFile(uri);
	}

	private void importFile(Uri fileUri) {
		if (fileUri == null)
			return;

		DocumentFile file = DocumentFile.fromSingleUri(getApplicationContext(), fileUri);
		if (file == null)
			return;

		String fileName = file.getName();
		ExternalFilesManager fileManager = new ExternalFilesManager(this);
		File externalFile = fileManager.getFile(fileName);

		try {
			InputStream inputStream = null;
			OutputStream outputStream = null;

			try {
				ContentResolver contentResolver = getContentResolver();
				inputStream = contentResolver.openInputStream(fileUri);
				outputStream = new FileOutputStream(externalFile);

				// Transfer bytes from in to out
				byte[] buf = new byte[4096];
				int len;
				while ((len = inputStream.read(buf)) > 0) {
					outputStream.write(buf, 0, len);
				}
			} finally {
				if (inputStream != null)
					inputStream.close();
				if (outputStream != null)
					outputStream.close();
			}
		} catch (IOException exception) {
			String message = exception.getMessage();
			if (message == null) {
				Log.e("importFile", "IOException", exception);
			} else {
				Log.e("importFile", message);
			}
		}
	}
}
