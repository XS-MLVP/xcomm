package com.xspcomm;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;

public class xspcommLoader {
  private static boolean LIB_LOADED = false;
  public static void loadLibraryFromJar(String path) throws IOException {
    InputStream inputStream = xspcommLoader.class.getResourceAsStream(path);
    if (inputStream == null) {
        throw new IOException("Could not find library: " + path);
    }
    File tempFile = File.createTempFile("lib", ".so");
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
        loadLibraryFromJar("/libxspcomm.so");
        loadLibraryFromJar("/libscalaxspcomm.so");
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
