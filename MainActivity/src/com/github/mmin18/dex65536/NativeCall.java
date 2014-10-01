package com.github.mmin18.dex65536;

import android.content.Context;

public class NativeCall {

	static {
		System.loadLibrary("my_lib");
	}

}
