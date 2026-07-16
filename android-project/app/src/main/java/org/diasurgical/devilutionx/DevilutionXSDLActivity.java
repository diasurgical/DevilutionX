package org.diasurgical.devilutionx;

import android.content.Intent;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.Locale;

public class DevilutionXSDLActivity extends SDLActivity {
	private ExternalFilesManager fileManager;
	private boolean noExit;

	protected void onCreate(Bundle savedInstanceState) {
		// windowSoftInputMode=adjustPan stopped working
		// for fullscreen apps after Android 7.0
		if (Build.VERSION.SDK_INT >= 25)
			trackVisibleSpace();

		// Force app to overlap with the display cutout
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
			getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;

		fileManager = new ExternalFilesManager(this);

		migrateSaveGames();

		super.onCreate(savedInstanceState);
	}

	/**
	 * On app launch make sure the game data is present
	 */
	protected void onStart() {
		super.onStart();

		if (isMissingGameData()) {
			Intent intent = new Intent(this, DataActivity.class);
			startActivity(intent);
			noExit = true;
			this.finish();
		}
	}

	/**
	 * When the user exits the game, use System.exit(0)
	 * to clear memory and prevent errors on restart
	 */
	protected void onDestroy() {
		super.onDestroy();

		if (!noExit) {
			System.exit(0);
		}
	}

	private void trackVisibleSpace() {
		this.getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
			@Override
			public void onGlobalLayout() {
				SurfaceView surface = mSurface;
				SurfaceHolder holder = surface.getHolder();

				if (!isIMEVisible()) {
					holder.setSizeFromLayout();
					return;
				}

				// Software keyboard may encroach on the app's visible space so
				// force the drawing surface to fit in the visible display frame
				Rect visibleSpace = new Rect();
				getVisibleSpaceAroundIME(visibleSpace);
				holder.setFixedSize(visibleSpace.width(), visibleSpace.height());
			}

			private boolean isIMEVisible() {
				// Window insets are not available before Android 11 so
				// we must always assume the software keyboard is visible
				if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R)
					return true;

				WindowInsets rootInsets = getWindow().getDecorView().getRootWindowInsets();
				return rootInsets.isVisible(WindowInsets.Type.ime());
			}

			private void getVisibleSpaceAroundIME(Rect visibleSpace) {
				if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
					getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleSpace);
					return;
				}

				WindowInsets rootInsets = getWindow().getDecorView().getRootWindowInsets();
				Insets imeInsets = rootInsets.getInsets(WindowInsets.Type.ime());
				DisplayMetrics realMetrics = new DisplayMetrics();
				getDisplay().getRealMetrics(realMetrics);

				visibleSpace.top = imeInsets.top;
				visibleSpace.left = imeInsets.left;
				visibleSpace.right = realMetrics.widthPixels - imeInsets.right;
				visibleSpace.bottom = realMetrics.heightPixels - imeInsets.bottom;
			}
		});
	}

	private boolean isMissingGameData() {
		String lang = Locale.getDefault().toString();
		if (lang.startsWith("pl") && !fileManager.hasFile("pl.mpq"))
			return true;
		if (lang.startsWith("ru") && !fileManager.hasFile("ru.mpq"))
			return true;
		File fonts_mpq = fileManager.getFile("/fonts.mpq");
		if (lang.startsWith("ko") || lang.startsWith("zh") || lang.startsWith("ja") || fonts_mpq.exists()) {
			if (!fonts_mpq.exists() || areFontsOutOfDate(fonts_mpq.getAbsolutePath()))
				return true;
		}

		return !fileManager.hasFile("diabdat.mpq") &&
				!fileManager.hasFile("DIABDAT.MPQ") &&
				!fileManager.hasFile("spawn.mpq");
	}

	private void migrateSaveGames() {
		File[] files = getFilesDir().listFiles();
		if (files == null)
			return;
		for (File internalFile : files) {
			fileManager.migrateFile(internalFile);
		}
	}

	/**
	 * This method is called by SDL using JNI.
	 */
	public String getLocale()
	{
		return Locale.getDefault().toString();
	}

	protected String[] getArguments() {
		String externalDir = fileManager.getExternalFilesDirectory();

		if (BuildConfig.DEBUG) {
			return new String[]{
				"--data-dir",
				externalDir,
				"--config-dir",
				externalDir,
				"--save-dir",
				externalDir,
				"--verbose",
			};
		}

		return new String[]{
			"--data-dir",
			externalDir,
			"--config-dir",
			externalDir,
			"--save-dir",
			externalDir
		};
	}

	protected String[] getLibraries() {
		return new String[]{
				"devilutionx"
		};
	}

	public static native boolean areFontsOutOfDate(String fonts_mpq);
}
