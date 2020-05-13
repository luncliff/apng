package dev.luncliff;

import java.nio.file.Paths;
import java.io.File;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;
import org.junit.jupiter.api.*;

/**
 * @see https://www.petrikainulainen.net/programming/testing/junit-5-tutorial-writing-our-first-test-class/
 */
class NativeTest {
    @BeforeAll
    static void printLibraryPath() {
        var libpath = System.getProperty("java.library.path");
        System.out.println(libpath);
    }

    @Test
    void checkEnvVarSet() {
        assertNotEquals(null, System.getenv("TEST"));
    }

    @Test
    void checkNotErrorHasMessage() {
        var code = 0;
        var message = Native.GetSystemMessage(code);
        assertNotEquals(null, message);
    }
}
