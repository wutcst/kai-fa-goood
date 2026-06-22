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

        boolean background = false;
        int argStart = 1;
        if ("server".equals(mode) && args.length > 1 && "--background".equals(args[1])) {
            background = true;
            argStart = 2;
            Path pidFile = runtimeDir.resolve("fireice_server.pid");
            Path logFile = runtimeDir.resolve("fireice_server.log");
            command.add("--pid-file");
            command.add(pidFile.toAbsolutePath().toString());
            command.add("--log-file");
            command.add(logFile.toAbsolutePath().toString());
        }

        if ("client".equals(mode) && args.length > argStart) {
            command.addAll(Arrays.asList(Arrays.copyOfRange(args, argStart, args.length)));
        }

        ProcessBuilder builder = new ProcessBuilder(command);
        builder.directory(runtimeDir.toFile());
        if (background) {
            Path logFile = runtimeDir.resolve("fireice_server.log");
            builder.redirectOutput(ProcessBuilder.Redirect.appendTo(logFile.toFile()));
            builder.redirectError(ProcessBuilder.Redirect.appendTo(logFile.toFile()));
            Process process = builder.start();
            Thread.sleep(1000L);
            Path pidFile = runtimeDir.resolve("fireice_server.pid");
            if (Files.isRegularFile(pidFile)) {
                String pid = new String(Files.readAllBytes(pidFile)).trim();
                System.out.println("Server started in background (PID " + pid + ").");
            } else {
                System.out.println("Server started in background (PID " + processId(process) + ").");
            }
            System.out.println("UDP port: 24567");
            System.out.println("Log: " + logFile.toAbsolutePath());
            System.out.println("Runtime dir: " + runtimeDir.toAbsolutePath());
            return;
        }

        builder.inheritIO();
        Process process = builder.start();
        System.exit(process.waitFor());
    }

    private static long processId(Process process) {
        try {
            java.lang.reflect.Method method = process.getClass().getMethod("pid");
            Object value = method.invoke(process);
            if (value instanceof Long) {
                return (Long) value;
            }
            if (value instanceof Integer) {
                return ((Integer) value).longValue();
            }
        } catch (ReflectiveOperationException ignored) {
            // Java 8 fallback below
        }
        return -1L;
    }

    static void printUsage() {
        System.out.println("Fire-Ice Online release launcher");
        System.out.println("  java -jar fire-ice-" + ReleaseVersion.VERSION + ".jar server [--background]");
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
