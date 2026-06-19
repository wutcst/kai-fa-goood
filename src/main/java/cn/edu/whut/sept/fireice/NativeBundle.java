package cn.edu.whut.sept.fireice;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.JarURLConnection;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

final class NativeBundle {
    private static final String RESOURCE_ROOT = "native/";

    private NativeBundle() {}

    static Path ensureExtracted(Path targetDir) throws IOException {
        Files.createDirectories(targetDir);

        URL location = NativeBundle.class.getProtectionDomain().getCodeSource().getLocation();
        if (location == null) {
            throw new IOException("Unable to locate launcher JAR.");
        }

        if ("file".equalsIgnoreCase(location.getProtocol()) && location.getPath().endsWith(".jar")) {
            try {
                extractFromJar(Paths.get(location.toURI()), targetDir);
            } catch (java.net.URISyntaxException e) {
                throw new IOException("Invalid launcher JAR location.", e);
            }
        } else if ("jar".equalsIgnoreCase(location.getProtocol())) {
            extractFromRunningJar(location, targetDir);
        } else {
            throw new IOException("Unsupported launcher location: " + location);
        }

        return targetDir;
    }

    private static void extractFromRunningJar(URL location, Path targetDir) throws IOException {
        JarURLConnection connection = (JarURLConnection) location.openConnection();
        try (JarFile jarFile = connection.getJarFile()) {
            Enumeration<JarEntry> entries = jarFile.entries();
            while (entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                if (entry.isDirectory() || !entry.getName().startsWith(RESOURCE_ROOT)) {
                    continue;
                }
                Path output = targetDir.resolve(entry.getName().substring(RESOURCE_ROOT.length()));
                if (entry.isDirectory()) {
                    Files.createDirectories(output);
                    continue;
                }
                Files.createDirectories(output.getParent());
                try (InputStream input = jarFile.getInputStream(entry)) {
                    Files.copy(input, output, StandardCopyOption.REPLACE_EXISTING);
                }
            }
        }
    }

    private static void extractFromJar(Path jarPath, Path targetDir) throws IOException {
        try (JarFile jarFile = new JarFile(jarPath.toFile())) {
            Enumeration<JarEntry> entries = jarFile.entries();
            while (entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                if (entry.isDirectory() || !entry.getName().startsWith(RESOURCE_ROOT)) {
                    continue;
                }
                Path output = targetDir.resolve(entry.getName().substring(RESOURCE_ROOT.length()));
                Files.createDirectories(output.getParent());
                try (InputStream input = jarFile.getInputStream(entry)) {
                    copyStream(input, output);
                }
            }
        }
    }

    private static void copyStream(InputStream input, Path output) throws IOException {
        Files.createDirectories(output.getParent());
        try (OutputStream out = Files.newOutputStream(output)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = input.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
        }
    }
}
