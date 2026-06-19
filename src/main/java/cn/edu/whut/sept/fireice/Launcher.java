package cn.edu.whut.sept.fireice;

import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * Release launcher for Fire-Ice Online.
 *
 * <p>Usage:
 *
 * <pre>
 * java -jar fire-ice-1.0.0.jar server
 * java -jar fire-ice-1.0.0.jar client [host] [role]
 * java -jar fire-ice-1.0.0.jar --version
 * </pre>
 */
public final class Launcher {
    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            printUsage();
            System.exit(1);
        }

        if ("--version".equals(args[0]) || "-v".equals(args[0])) {
            System.out.println(ReleaseVersion.NAME + " " + ReleaseVersion.VERSION);
            return;
        }

        String mode = args[0].toLowerCase(Locale.ROOT);
        if (!"server".equals(mode) && !"client".equals(mode)) {
            printUsage();
            System.exit(1);
        }

        Path runtimeDir = Paths.get(System.getProperty("java.io.tmpdir"), "fire-ice-runtime", mode + "-" + processId());
        NativeBundle.ensureExtracted(runtimeDir);

        Path executable = runtimeDir.resolve("server".equals(mode) ? "fireice_server.exe" : "fireice_client.exe");
        if (!Files.isRegularFile(executable)) {
            throw new IOException("Missing executable: " + executable);
        }

        List<String> command = new ArrayList<>();
        command.add(executable.toAbsolutePath().toString());
        if ("client".equals(mode) && args.length > 1) {
            command.addAll(Arrays.asList(Arrays.copyOfRange(args, 1, args.length)));
        }

        ProcessBuilder builder = new ProcessBuilder(command);
        builder.directory(runtimeDir.toFile());
        builder.inheritIO();
        Process process = builder.start();
        System.exit(process.waitFor());
    }

    static void printUsage() {
        System.out.println("Fire-Ice Online release launcher");
        System.out.println("  java -jar fire-ice-" + ReleaseVersion.VERSION + ".jar server");
        System.out.println("  java -jar fire-ice-" + ReleaseVersion.VERSION + ".jar client [host] [role]");
        System.out.println("  java -jar fire-ice-" + ReleaseVersion.VERSION + ".jar --version");
    }

    private static String processId() {
        String runtimeName = ManagementFactory.getRuntimeMXBean().getName();
        int at = runtimeName.indexOf('@');
        if (at > 0) {
            return runtimeName.substring(0, at);
        }
        return Long.toString(System.currentTimeMillis());
    }
}
