package com.xspcomm;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.FileOutputStream;
import java.nio.file.Files;

public class xspcomm {
  private static boolean LIB_LOADED = false;
  public static void loadLibraryFromJar(String path) throws IOException {
    InputStream inputStream = xspcomm.class.getResourceAsStream(path);
    if (inputStream == null) {
        throw new IOException("Could not find library: " + path);
    }
    String suffix = path.substring(path.lastIndexOf('.'));
    File tempFile = File.createTempFile("lib", suffix);
    tempFile.deleteOnExit();
    try (FileOutputStream outputStream = new FileOutputStream(tempFile)) {
        byte[] buffer = new byte[1024];
        int bytesRead;
        while ((bytesRead = inputStream.read(buffer)) != -1) {
            outputStream.write(buffer, 0, bytesRead);
        }
    }
    System.load(tempFile.getAbsolutePath());
  }
  static {
    try {
        String os = System.getProperty("os.name").toLowerCase();
        String coreLibName;
        String jniLibName;
        if (os.contains("mac")) {
            coreLibName = "/libxspcomm.dylib";
            jniLibName  = "/libjavaxspcomm.jnilib";

        } else if (os.contains("nux")) {
            coreLibName = "/libxspcomm.so";
            jniLibName  = "/libjavaxspcomm.so";

        } else {
            throw new UnsupportedOperationException("Unsupported OS: " + os);
        }
        loadLibraryFromJar(coreLibName);
        loadLibraryFromJar(jniLibName);
        LIB_LOADED = true;
    } catch (Exception e) {
        System.err.println("Error load so fail:");
        e.printStackTrace();
        LIB_LOADED = false;
    }
  }

  public static void main(String[] args) {
    if(init()){
      System.out.println("xspcomm is usable!");
    } else {
      System.out.println("xspcomm is broken!");
    }
    System.out.println("version: " + Util.version());
  }

  public static boolean init() {
    return LIB_LOADED;
  }
}
