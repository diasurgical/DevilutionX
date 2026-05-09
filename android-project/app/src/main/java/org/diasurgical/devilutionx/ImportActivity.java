package org.diasurgical.devilutionx;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.Nullable;
import android.os.Bundle;
import android.support.annotation.RequiresApi;
import android.support.v4.provider.DocumentFile;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.URLSpan;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public class ImportActivity extends Activity {

	private static final int IMPORT_REQUEST_CODE = 0xD1AB70;
	private static final String TV_STUB_PACKAGE = "com.android.tv.frameworkpackagestubs";
	private static final String RECOMMENDED_FILE_BROWSER = "AnExplorer";
	private static final String RECOMMENDED_FILE_BROWSER_PACKAGE = "dev.dworks.apps.anexplorer";
	private boolean userConfirmed = false;

	@RequiresApi(api = Build.VERSION_CODES.KITKAT)
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		if (findFileBrowser()) {
			runFileBrowser();
		} else {
			recommendFileBrowser();
		}
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

	private boolean findFileBrowser() {
		Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
		intent.addCategory(Intent.CATEGORY_OPENABLE);
		intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
		intent.setType("*/*");

		PackageManager packageManager = getPackageManager();
		ComponentName defaultPackage = intent.resolveActivity(packageManager);
		return defaultPackage != null && !TV_STUB_PACKAGE.equals(defaultPackage.getPackageName());
	}

	private void recommendFileBrowser()
	{
		String noFileBrowserText = getString(R.string.no_file_browser);
		SpannableString message;

		boolean isTelevision = getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK);
		if (isTelevision) {
			String recommendation = getString(R.string.recommended_file_browser, RECOMMENDED_FILE_BROWSER);
			String messageText = noFileBrowserText + " " + recommendation;
			message = new SpannableString(messageText);

			int index = messageText.indexOf(RECOMMENDED_FILE_BROWSER);
			if (index >= 0) {
				String url = "market://details?id=" + RECOMMENDED_FILE_BROWSER_PACKAGE;
				URLSpan urlSpan = new URLSpan(url);
				message.setSpan(urlSpan, index, index + RECOMMENDED_FILE_BROWSER.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
			}
		} else {
			message = new SpannableString(noFileBrowserText);
		}

		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(message);
		builder.setPositiveButton("OK", null);
		builder.setOnDismissListener(dialog -> {
			if (findFileBrowser()) {
				runFileBrowser();
			} else if (!isFinishing()) {
				finish();
			}
		});

		AlertDialog dialog = builder.create();
		dialog.show();

		View view = dialog.findViewById(android.R.id.message);
		if (view instanceof TextView) {
			TextView textView = (TextView)view;
			textView.setMovementMethod(LinkMovementMethod.getInstance());
		}
	}

	private void runFileBrowser() {
		ExternalFilesManager fileManager = new ExternalFilesManager(this);
		String externalFilesDir =  fileManager.getExternalFilesDirectory();

		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(getString(R.string.import_data_info, externalFilesDir));
		builder.setPositiveButton(R.string.ok_button, (dialog, which) -> {
			userConfirmed = true;
			Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
			intent.addCategory(Intent.CATEGORY_OPENABLE);
			intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
			intent.setType("*/*");
			startActivityForResult(intent, IMPORT_REQUEST_CODE);
		});
		builder.setOnDismissListener(dialog -> {
			if (!userConfirmed && !isFinishing()) finish();
		});

		AlertDialog dialog = builder.create();
		dialog.show();
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
