package com.xspcomm;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;

public class xspcomm {
  private static boolean LIB_LOADED = false;
  static {
    try {
      InputStream in = xspcomm.class.getResourceAsStream("/libscalaxspcomm.so");
      File tempLib = File.createTempFile("libscalaxspcomm", ".so");
      tempLib.deleteOnExit(); // The file is deleted when the JVM exits
      OutputStream out = new FileOutputStream(tempLib);
      byte[] buffer = new byte[1024];
      int read;
      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
      out.close();
      in.close();
      System.load(tempLib.getAbsolutePath());
      LIB_LOADED = true;
    } catch (Exception e) {
      System.err.println("Error loading libscalaxspcomm.so:");
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
