package dev.luncliff;

import java.lang.System;

public class Native {
    static {
        System.loadLibrary("engine");
    }

    static native String GetSystemMessage(int code);
}
