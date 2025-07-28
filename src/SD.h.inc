/*
  SD - Refactored SD card library for STM32F4
  
  Refactored to match LittleFS interface pattern, inheriting from FS.h
  for unified filesystem access.
  
  Based on original SD library:
  License: GNU General Public License V3
  (C) Copyright 2010 SparkFun Electronics
*/

#ifndef __SD_H__
#define __SD_H__

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include "utility/SdFat.h"
#include "utility/SdFatUtil.h"

// Configuration - can be overridden at compile time
#ifndef SD_MAX_OPEN_FILES
#define SD_MAX_OPEN_FILES 4
#endif

#ifndef SD_MAX_OPEN_DIRS
#define SD_MAX_OPEN_DIRS 2
#endif

// Default CS pin for SD card
#ifndef SD_CHIP_SELECT_PIN
#define SD_CHIP_SELECT_PIN 10
#endif

// Path component buffer length for directory traversal
#define MAX_COMPONENT_LEN 12
#define PATH_COMPONENT_BUFFER_LEN (MAX_COMPONENT_LEN + 1)

// Forward declarations
class SD;

// SDFile class implementing FileImpl interface
class SDFile : public FileImpl
{
private:
    // Private constructors - only SD class can create instances
    SDFile(SdFile *filein, const char *name);
    SDFile(SdFile *dirin, const char *name, bool isDir);
    
    friend class SD;

public:
    virtual ~SDFile();

    // Core file operations
    virtual size_t read(void *buf, size_t nbyte) override;
    virtual size_t write(const void *buf, size_t size) override;
    virtual int available() override;
    virtual int peek() override;
    virtual void flush() override;
    virtual bool truncate(uint64_t size = 0) override;
    virtual bool seek(uint64_t pos, int mode) override;
    virtual uint64_t position() override;
    virtual uint64_t size() override;
    virtual void close() override;
    virtual bool isOpen() override;
    virtual const char *name() override;
    virtual bool isDirectory() override;
    virtual File openNextFile(uint8_t mode = 0) override;
    virtual void rewindDirectory(void) override;

    // Date/time operations
    virtual bool getCreateTime(DateTimeFields &tm) override;
    virtual bool getModifyTime(DateTimeFields &tm) override;
    virtual bool setCreateTime(const DateTimeFields &tm) override;
    virtual bool setModifyTime(const DateTimeFields &tm) override;

private:
    SdFile *_file;
    char _name[PATH_COMPONENT_BUFFER_LEN];
    char _fullpath[128];
    bool _isDirectory;
    SD *_parent;  // Reference to parent SD object for directory operations

    // Helper methods
    uint32_t getFileTimestamp(bool creation);
    bool setFileTimestamp(const DateTimeFields &tm, bool creation);
    void extractFileName(const char *fullpath);
};

// SD class implementing FS interface
class SD : public FS
{
public:
    SD() : _card(), _volume(), _root(), _initialized(false) {}
    virtual ~SD() {}

    // Initialization
    bool begin(uint8_t csPin = SD_CHIP_SELECT_PIN);
    bool begin(uint32_t clock, uint8_t csPin);
    void end();

    // FS interface implementation
    virtual File open(const char *filepath, uint8_t mode = FILE_READ) override;
    virtual bool exists(const char *filepath) override;
    virtual bool mkdir(const char *filepath) override;
    virtual bool rename(const char *oldfilepath, const char *newfilepath) override;
    virtual bool remove(const char *filepath) override;
    virtual bool rmdir(const char *filepath) override;
    virtual uint64_t usedSize() override;
    virtual uint64_t totalSize() override;
    virtual bool format() override;
    virtual bool mediaPresent() override;
    virtual const char *name() override;

    // SD-specific methods
    bool cardBegin(uint8_t csPin = SD_CHIP_SELECT_PIN) { return begin(csPin); }
    bool cardBegin(uint32_t clock, uint8_t csPin) { return begin(clock, csPin); }
    
    // Access to underlying SdFat objects (for advanced users)
    Sd2Card* card() { return &_card; }
    SdVolume* volume() { return &_volume; }
    SdFile* root() { return &_root; }

private:
    Sd2Card _card;
    SdVolume _volume;
    SdFile _root;
    bool _initialized;

    // Internal helper methods
    SdFile getParentDir(const char *filepath, int *index);
    bool walkPath(const char *filepath, 
                  bool(*callback)(SdFile& parentDir, const char *component, 
                                bool isLastComponent, void *context),
                  void *context = nullptr);
    
    // Path traversal callbacks
    static bool pathExistsCallback(SdFile& parentDir, const char *component, 
                                 bool isLastComponent, void *context);
    static bool makeDirCallback(SdFile& parentDir, const char *component, 
                              bool isLastComponent, void *context);
    static bool removeCallback(SdFile& parentDir, const char *component, 
                             bool isLastComponent, void *context);
    static bool rmdirCallback(SdFile& parentDir, const char *component, 
                            bool isLastComponent, void *context);

    // Path parsing utilities
    bool getNextPathComponent(const char *path, unsigned int *offset, char *buffer);
    
    // Size calculation helpers
    uint64_t calculateDirectorySize(SdFile &dir);
    
    friend class SDFile;
};

// Global SD instance
extern SD SDStorage;

// Compatibility aliases (can be removed if not needed)
#define SDCard SDStorage

#endif // __SD_H__
