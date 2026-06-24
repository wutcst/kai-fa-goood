package cn.edu.whut.sept.fireice;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

class LauncherTest {
    @Test
    void releaseVersionIsDefined() {
        assertEquals("1.1.0", ReleaseVersion.VERSION);
        assertTrue(ReleaseVersion.NAME.contains("Fire-Ice"));
    }
}
