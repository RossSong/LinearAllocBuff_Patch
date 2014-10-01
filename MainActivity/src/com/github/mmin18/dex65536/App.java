package com.github.mmin18.dex65536;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.lang.reflect.Field;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import dalvik.system.DexClassLoader;

public class App extends Application {

	@Override
	public void onCreate() {
		super.onCreate();
	}
}
