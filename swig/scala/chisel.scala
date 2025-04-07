package com.xspcomm

import java.io.{File, PrintWriter}
import java.nio.file.{Files, Paths}
import scala.io.Source
import scala.sys.process._
import scala.reflect.runtime.universe._
import java.security.MessageDigest
import java.net.URL
import java.net.URLClassLoader


object chiselUT {

  def debug(msg: String): Unit = {
    println(s"[DEBUG] $msg")
  }

  def generateDUT[A: TypeTag](issueVerilog: (String, String) => Unit,
                  workDir: String = ".pickerChiselUT",
                  forceRebuild: Boolean = false,
                  autoRebuild: Boolean = true,
                  pickerExArgs: String = "",
                  ): BaseDUTTrait = {
    // Files need to check or generate
    val dutClassName = typeOf[A].typeSymbol.fullName.split('.').last.stripSuffix("$")
    val verilogFile  = Paths.get(workDir, dutClassName, s"$dutClassName.v")
    val scalaJarFile = Paths.get(workDir, "DUT", dutClassName, s"UT_$dutClassName-scala.jar")
    val recomTagFile = Paths.get(workDir, dutClassName, s"$dutClassName.tag")
    val classWorkDir = Paths.get(workDir, dutClassName)
    new File(classWorkDir.toString).mkdirs()
    // Check if Need to Rebuild
    val rebuild = forceRebuild || !scalaJarFile.toFile.exists() || !verilogFile.toFile.exists() || !recomTagFile.toFile.exists() ||
      (hasFileChanged[A](verilogFile.toFile, recomTagFile.toFile) && autoRebuild)
    // build if needed
    if(rebuild){
      deleteDirectory(Paths.get(workDir, "DUT").toFile)
      debug(s"Re-Compile DUT.")
      val scalaXspcommJar: String = "picker --show_xcom_lib_location_scala".!!
      val scalaXspcommJarFile = scalaXspcommJar.split(" ").headOption.getOrElse("").trim
      if (!Paths.get(scalaXspcommJarFile).toFile.exists()) {
        throw new RuntimeException("Cannot find Scala XspComm library. Please check your picker scala environment.")
      }
      debug(s"Generating Verilog for $dutClassName...")
      issueVerilog(classWorkDir.toString(), verilogFile.toString())
      if(verilogFile.toFile.exists()) {
        saveMetadata[A](verilogFile.toFile, recomTagFile.toFile)
      } else {
        throw new RuntimeException(s"Verilog file $verilogFile does not exist after generation.")
      }
      debug(s"Converting Verilog to Scala JAR using picker...")
      val pickerCommand = s"picker export $verilogFile --tdir $workDir/DUT/ $pickerExArgs --lang scala"
      val pickerResult = pickerCommand.!
      if (pickerResult != 0) {
        throw new RuntimeException(s"Picker command failed with exit code $pickerResult")
      }
      if (!scalaJarFile.toFile.exists()) {
        throw new RuntimeException(s"Scala JAR file $scalaJarFile does not exist after generation.")
      }
    } else {
      debug(s"Use Cached Scala JAR: $scalaJarFile")
    }
    // Load the Scala JAR and instantiate the DUT
    val classLoader = loadJars(Seq(
      Paths.get(scalaJarFile.toString).toFile,
    ))
    val dutClass = classLoader.loadClass(s"com.ut.UT_$dutClassName")
    val dutInstance = dutClass.getDeclaredConstructor().newInstance().asInstanceOf[BaseDUTTrait]
    dutInstance
  }

  // Support functions
  def loadJars(jarFiles: Seq[File]): URLClassLoader = {
    val jarUrls: Array[URL] = jarFiles.map(_.toURI.toURL).toArray
    new URLClassLoader(jarUrls, ClassLoader.getSystemClassLoader)
  }

  def hasFileChanged[T: TypeTag](file: File, metadataFile: File): Boolean = {
    val currentLastModified = file.lastModified()
    val currentHash = computeFileHash(file)
    val classHash = computeClassHash[T]()
    readMetadata(metadataFile) match {
      case Some((savedLastModified, savedHash, classHash)) =>
        currentLastModified != savedLastModified || currentHash != savedHash ||
        classHash != classHash
      case None =>
        true
    }
  }

  def computeFileHash(file: File): String = {
    val digest = MessageDigest.getInstance("SHA-256")
    val bytes = Files.readAllBytes(file.toPath)
    digest.digest(bytes).map("%02x".format(_)).mkString
  }

  def saveMetadata[T: TypeTag](file: File, metadataFile: File): Unit = {
    val lastModified = file.lastModified()
    val fileHash = computeFileHash(file)
    val classHash = computeClassHash[T]()
    val writer = new PrintWriter(metadataFile)
    try {
      writer.println(s"lastModified=$lastModified")
      writer.println(s"fileHash=$fileHash")
      writer.println(s"classHash=$classHash")
    } finally {
      writer.close()
    }
  }

  def readMetadata(metadataFile: File): Option[(Long, String, String)] = {
    if (!metadataFile.exists()) return None
    val lines = Source.fromFile(metadataFile).getLines().toList
    val lastModified = lines.find(_.startsWith("lastModified=")).map(_.split("=")(1).toLong)
    val fileHash = lines.find(_.startsWith("fileHash=")).map(_.split("=")(1))
    val classHash = lines.find(_.startsWith("classHash=")).map(_.split("=")(1))
    for {
      lm <- lastModified
      fh <- fileHash
      ch <- classHash
    } yield (lm, fh, ch)
  }

  def computeClassHash[T: TypeTag](): String = {
    val structureHash = computeStructureHash[T]
    val bytecodeHash = computeBytecodeHash[T]
    sha1(structureHash + bytecodeHash)
  }

  private implicit val methodSymbolOrdering: Ordering[MethodSymbol] = 
    Ordering.by[MethodSymbol, String](_.name.decodedName.toString)

  private def computeStructureHash[T: TypeTag]: String = {
    val tpe = typeOf[T]
    val info = List(
      s"CLASS_NAME=${tpe.typeSymbol.fullName}",
      s"FIELDS=${tpe.decls.collect { case m: MethodSymbol if m.isGetter => m }.toList.sorted}",
      s"METHODS=${tpe.decls.filter(_.isMethod).map(_.name.decodedName).toSet}",
      s"PARENTS=${tpe.baseClasses.map(_.fullName).sorted}"
    ).mkString("|")
    sha1(info)
  }

  private def computeBytecodeHash[T: TypeTag]: String = {
    val className = typeOf[T].typeSymbol.fullName
    val resourcePath = className.replace('.', '/') + ".class"
    val url: URL = Option(getClass.getClassLoader.getResource(resourcePath))
      .getOrElse(throw new Exception(s"Cannot find source file: $resourcePath"))
    val filePath = Paths.get(url.toURI)
    val creationTime = Files.getAttribute(filePath, "creationTime")
    val inputStream = url.openStream()
    val bytecode = try {
      inputStream.readAllBytes()
    } finally {
      inputStream.close()
    }
    sha1(bytecode)
  }

  private def sha1(data: String): String = {
    val digest = MessageDigest.getInstance("SHA-1").digest(data.getBytes("UTF-8"))
    digest.map("%02x".format(_)).mkString
  }

  private def sha1(bytes: Array[Byte]): String = {
    val digest = MessageDigest.getInstance("SHA-1").digest(bytes)
    digest.map("%02x".format(_)).mkString
  }

  private def deleteDirectory(directory: File): Boolean = {
    if (directory.isDirectory) {
      directory.listFiles().foreach(deleteDirectory)
    }
    directory.delete()
  }
}