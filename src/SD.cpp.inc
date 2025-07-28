/*
  SD - Refactored SD card library for STM32F4
  
  Refactored to match LittleFS interface pattern, inheriting from FS.h
  for unified filesystem access.
  
  Based on original SD library:
  License: GNU General Public License V3
  (C) Copyright 2010 SparkFun Electronics
*/

#include "SD.h"

// Global SD instance
SD SDStorage;

// =============================================================================
// SDFile Implementation
// =============================================================================

SDFile::SDFile(SdFile *filein, const char *name) 
    : _file(filein), _isDirectory(false), _parent(&SDStorage)
{
    if (!filein || !name) {
        _file = nullptr;
        _name[0] = '\0';
        _fullpath[0] = '\0';
        return;
    }
    
    strlcpy(_fullpath, name, sizeof(_fullpath));
    extractFileName(name);
    _isDirectory = _file->isDir();
}

SDFile::SDFile(SdFile *dirin, const char *name, bool isDir) 
    : _file(dirin), _isDirectory(isDir), _parent(&SDStorage)
{
    if (!dirin || !name) {
        _file = nullptr;
        _name[0] = '\0';
        _fullpath[0] = '\0';
        return;
    }
    
    strlcpy(_fullpath, name, sizeof(_fullpath));
    extractFileName(name);
}

SDFile::~SDFile()
{
    close();
}

size_t SDFile::read(void *buf, size_t nbyte)
{
    if (!_file || !buf || nbyte == 0 || _isDirectory) return 0;
    
    int result = _file->read(buf, nbyte);
    return (result < 0) ? 0 : result;
}

size_t SDFile::write(const void *buf, size_t size)
{
    if (!_file || !buf || size == 0 || _isDirectory) return 0;
    
    _file->clearWriteError();
    size_t written = _file->write(buf, size);
    if (_file->getWriteError()) {
        return 0;
    }
    return written;
}

int SDFile::available()
{
    if (!_file || _isDirectory) return 0;
    
    uint32_t fileSize = _file->fileSize();
    uint32_t pos = _file->curPosition();
    
    uint32_t avail = (fileSize > pos) ? (fileSize - pos) : 0;
    return (avail > 0x7FFF) ? 0x7FFF : avail;
}

int SDFile::peek()
{
    if (!_file || _isDirectory) return -1;
    
    int c = _file->read();
    if (c != -1) {
        _file->seekCur(-1);
    }
    return c;
}

void SDFile::flush()
{
    if (_file && !_isDirectory) {
        _file->sync();
    }
}

bool SDFile::truncate(uint64_t size)
{
    if (!_file || _isDirectory) return false;
    
    return _file->truncate(size);
}

bool SDFile::seek(uint64_t pos, int mode)
{
    if (!_file || _isDirectory) return false;
    
    switch (mode) {
        case SeekSet:
            return _file->seekSet(pos);
        case SeekCur:
            return _file->seekCur(pos);
        case SeekEnd:
            if (pos == 0) {
                return _file->seekEnd();
            } else {
                // SdFat seekEnd() doesn't take offset, so seek from end manually
                uint32_t fileSize = _file->fileSize();
                if (pos > fileSize) return false;
                return _file->seekSet(fileSize - pos);
            }
        default:
            return false;
    }
}

uint64_t SDFile::position()
{
    if (!_file || _isDirectory) return 0;
    return _file->curPosition();
}

uint64_t SDFile::size()
{
    if (!_file) return 0;
    if (_isDirectory) {
        // For directories, return number of entries
        dir_t entry;
        uint32_t count = 0;
        _file->rewind();
        while (_file->readDir(&entry) > 0) {
            if (entry.name[0] != DIR_NAME_FREE && 
                entry.name[0] != DIR_NAME_DELETED &&
                entry.name[0] != '.') {
                count++;
            }
        }
        return count;
    }
    return _file->fileSize();
}

void SDFile::close()
{
    if (_file) {
        _file->close();
        delete _file;
        _file = nullptr;
    }
}

bool SDFile::isOpen()
{
    return _file && _file->isOpen();
}

const char *SDFile::name()
{
    return _name;
}

bool SDFile::isDirectory()
{
    return _isDirectory;
}

File SDFile::openNextFile(uint8_t mode)
{
    if (!_file || !_isDirectory) return File();
    
    dir_t entry;
    
    // Read directory entries until we find a valid one
    while (_file->readDir(&entry) > 0) {
        // Skip free, deleted, and dot entries
        if (entry.name[0] == DIR_NAME_FREE) break;
        if (entry.name[0] == DIR_NAME_DELETED || entry.name[0] == '.') continue;
        
        // Skip non-files and non-directories
        if (!DIR_IS_FILE_OR_SUBDIR(&entry)) continue;
        
        // Extract filename
        char filename[13];
        _file->dirName(entry, filename);
        
        // Build full path
        char fullpath[128];
        strlcpy(fullpath, _fullpath, sizeof(fullpath));
        size_t len = strlen(fullpath);
        if (len > 0 && fullpath[len - 1] != '/' && len < sizeof(fullpath) - 2) {
            fullpath[len++] = '/';
            fullpath[len] = '\0';
        }
        strlcpy(fullpath + len, filename, sizeof(fullpath) - len);
        
        // Open the file/directory
        SdFile *newFile = new SdFile();
        if (!newFile) return File();
        
        if (newFile->open(_file, filename, mode)) {
            return File(new SDFile(newFile, fullpath, newFile->isDir()));
        }
        
        delete newFile;
    }
    
    return File();
}

void SDFile::rewindDirectory()
{
    if (_file && _isDirectory) {
        _file->rewind();
    }
}

bool SDFile::getCreateTime(DateTimeFields &tm)
{
    if (!_file) return false;
    
    uint32_t timestamp = getFileTimestamp(true);
    if (timestamp == 0) return false;
    
    breakTime(timestamp, tm);
    return true;
}

bool SDFile::getModifyTime(DateTimeFields &tm)
{
    if (!_file) return false;
    
    uint32_t timestamp = getFileTimestamp(false);
    if (timestamp == 0) return false;
    
    breakTime(timestamp, tm);
    return true;
}

bool SDFile::setCreateTime(const DateTimeFields &tm)
{
    if (!_file) return false;
    if (tm.year < 80 || tm.year > 207) return false;
    
    return setFileTimestamp(tm, true);
}

bool SDFile::setModifyTime(const DateTimeFields &tm)
{
    if (!_file) return false;
    if (tm.year < 80 || tm.year > 207) return false;
    
    return setFileTimestamp(tm, false);
}

uint32_t SDFile::getFileTimestamp(bool creation)
{
    if (!_file) return 0;
    
    dir_t entry;
    if (!_file->dirEntry(&entry)) return 0;
    
    if (creation) {
        // Creation time (if supported)
        return 0; // SdFat may not support creation time
    } else {
        // Modification time
        uint16_t date = entry.lastWriteDate;
        uint16_t time = entry.lastWriteTime;
        
        if (date == 0 && time == 0) return 0;
        
        // Convert FAT date/time to Unix timestamp
        DateTimeFields dt;
        dt.year = ((date >> 9) & 0x7F) + 80; // Years since 1980, offset to 1900
        dt.mon = (date >> 5) & 0x0F;
        dt.mday = date & 0x1F;
        dt.hour = (time >> 11) & 0x1F;
        dt.min = (time >> 5) & 0x3F;
        dt.sec = (time & 0x1F) * 2;
        
        return makeTime(dt);
    }
}

bool SDFile::setFileTimestamp(const DateTimeFields &tm, bool creation)
{
    // SdFat timestamp setting is complex and may require special handling
    // This is a simplified implementation
    return false; // Not easily supported by SdFat
}

void SDFile::extractFileName(const char *fullpath)
{
    const char *p = strrchr(fullpath, '/');
    if (p) {
        strlcpy(_name, p + 1, sizeof(_name));
    } else {
        strlcpy(_name, fullpath, sizeof(_name));
    }
}

// =============================================================================
// SD Implementation
// =============================================================================

bool SD::begin(uint8_t csPin)
{
    if (_root.isOpen()) {
        _root.close();
    }
    
    _initialized = _card.init(SPI_HALF_SPEED, csPin) &&
                   _volume.init(_card) &&
                   _root.openRoot(_volume);
    
    return _initialized;
}

bool SD::begin(uint32_t clock, uint8_t csPin)
{
    if (_root.isOpen()) {
        _root.close();
    }
    
    _initialized = _card.init(SPI_HALF_SPEED, csPin) &&
                   _card.setSpiClock(clock) &&
                   _volume.init(_card) &&
                   _root.openRoot(_volume);
    
    return _initialized;
}

void SD::end()
{
    _root.close();
    _initialized = false;
}

File SD::open(const char *filepath, uint8_t mode)
{
    if (!filepath || strlen(filepath) == 0 || !_initialized) {
        return File();
    }
    
    int pathIndex = 0;
    SdFile parentDir = getParentDir(filepath, &pathIndex);
    
    if (!parentDir.isOpen()) {
        return File();
    }
    
    const char *filename = filepath + pathIndex;
    if (!filename[0]) {
        // Opening the directory itself
        SdFile *dir = new SdFile();
        if (!dir) return File();
        
        *dir = parentDir; // Copy the directory
        return File(new SDFile(dir, filepath, true));
    }
    
    SdFile *file = new SdFile();
    if (!file) {
        parentDir.close();
        return File();
    }
    
    bool success = false;
    if (mode == FILE_READ) {
        success = file->open(parentDir, filename, O_READ);
    } else {
        success = file->open(parentDir, filename, O_RDWR | O_CREAT);
        if (success && mode == FILE_WRITE) {
            file->seekSet(file->fileSize()); // Append mode
        }
    }
    
    parentDir.close();
    
    if (success) {
        return File(new SDFile(file, filepath));
    } else {
        delete file;
        return File();
    }
}

bool SD::exists(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0 || !_initialized) {
        return false;
    }
    
    return walkPath(filepath, pathExistsCallback, nullptr);
}

bool SD::mkdir(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0 || !_initialized) {
        return false;
    }
    
    return walkPath(filepath, makeDirCallback, nullptr);
}

bool SD::rename(const char *oldfilepath, const char *newfilepath)
{
    if (!oldfilepath || !newfilepath || 
        strlen(oldfilepath) == 0 || strlen(newfilepath) == 0 || 
        !_initialized) {
        return false;
    }
    
    // SdFat doesn't have a direct rename function
    // This would require copying and deleting
    // For now, return false
    return false;
}

bool SD::remove(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0 || !_initialized) {
        return false;
    }
    
    return walkPath(filepath, removeCallback, nullptr);
}

bool SD::rmdir(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0 || !_initialized) {
        return false;
    }
    
    return walkPath(filepath, rmdirCallback, nullptr);
}

uint64_t SD::usedSize()
{
    if (!_initialized) return 0;
    
    // Calculate used space by traversing filesystem
    return calculateDirectorySize(_root);
}

uint64_t SD::totalSize()
{
    if (!_initialized) return 0;
    
    // Get total card capacity - SdFat uses cardSize() method
    return (uint64_t)_card.cardSize() * 512ULL; // Card size in 512-byte blocks
}

bool SD::format()
{
    if (!_initialized) return false;
    
    // SdFat formatting would require special setup
    // This is a complex operation that may need platform-specific code
    return false;
}

bool SD::mediaPresent()
{
    return _initialized; // If initialization succeeded, card is present and ready
}

const char *SD::name()
{
    return "SD";
}

SdFile SD::getParentDir(const char *filepath, int *index)
{
    SdFile d1, d2;
    d1.openRoot(_volume);
    
    SdFile *parent = &d1;
    SdFile *subdir = &d2;
    
    const char *origpath = filepath;
    
    while (strchr(filepath, '/')) {
        if (filepath[0] == '/') {
            filepath++;
            continue;
        }
        
        if (!strchr(filepath, '/')) {
            break;
        }
        
        uint8_t idx = strchr(filepath, '/') - filepath;
        if (idx > 12) idx = 12;
        
        char subdirname[13];
        strncpy(subdirname, filepath, idx);
        subdirname[idx] = 0;
        
        subdir->close();
        if (!subdir->open(parent, subdirname, O_READ)) {
            return SdFile();
        }
        
        filepath += idx;
        parent->close();
        
        SdFile *temp = parent;
        parent = subdir;
        subdir = temp;
    }
    
    *index = (int)(filepath - origpath);
    return *parent;
}

bool SD::walkPath(const char *filepath, 
                  bool(*callback)(SdFile& parentDir, const char *component, 
                                bool isLastComponent, void *context),
                  void *context)
{
    SdFile subfile1, subfile2;
    char buffer[PATH_COMPONENT_BUFFER_LEN];
    unsigned int offset = 0;
    
    SdFile *parent = &_root;
    SdFile *child = &subfile1;
    
    while (true) {
        bool moreComponents = getNextPathComponent(filepath, &offset, buffer);
        
        if (!callback(*parent, buffer, !moreComponents, context)) {
            if (parent != &_root) {
                parent->close();
            }
            return false;
        }
        
        if (!moreComponents) {
            break;
        }
        
        bool exists = child->open(*parent, buffer, O_RDONLY);
        
        if (parent != &_root) {
            parent->close();
        }
        
        if (exists) {
            if (parent == &_root) {
                parent = &subfile2;
            }
            
            SdFile *temp = parent;
            parent = child;
            child = temp;
        } else {
            return false;
        }
    }
    
    if (parent != &_root) {
        parent->close();
    }
    
    return true;
}

bool SD::getNextPathComponent(const char *path, unsigned int *offset, char *buffer)
{
    int bufferOffset = 0;
    int pos = *offset;
    
    if (path[pos] == '/') {
        pos++;
    }
    
    while (bufferOffset < MAX_COMPONENT_LEN &&
           path[pos] != '/' && path[pos] != '\0') {
        buffer[bufferOffset++] = path[pos++];
    }
    
    buffer[bufferOffset] = '\0';
    
    if (path[pos] == '/') {
        pos++;
    }
    
    *offset = pos;
    return (path[pos] != '\0');
}

bool SD::pathExistsCallback(SdFile& parentDir, const char *component, 
                           bool isLastComponent, void *context)
{
    SdFile child;
    bool exists = child.open(parentDir, component, O_RDONLY);
    if (exists) {
        child.close();
    }
    return exists;
}

bool SD::makeDirCallback(SdFile& parentDir, const char *component, 
                        bool isLastComponent, void *context)
{
    SdFile child;
    
    // Check if already exists
    if (child.open(parentDir, component, O_RDONLY)) {
        child.close();
        return true;
    }
    
    // Create directory
    return child.makeDir(parentDir, component);
}

bool SD::removeCallback(SdFile& parentDir, const char *component, 
                       bool isLastComponent, void *context)
{
    if (isLastComponent) {
        return SdFile::remove(parentDir, component);
    }
    return true;
}

bool SD::rmdirCallback(SdFile& parentDir, const char *component, 
                      bool isLastComponent, void *context)
{
    if (isLastComponent) {
        SdFile file;
        if (!file.open(parentDir, component, O_READ)) {
            return false;
        }
        return file.rmDir();
    }
    return true;
}

uint64_t SD::calculateDirectorySize(SdFile &dir)
{
    dir_t entry;
    uint64_t totalSize = 0;
    
    dir.rewind();
    while (dir.readDir(&entry) > 0) {
        if (entry.name[0] == DIR_NAME_FREE) break;
        if (entry.name[0] == DIR_NAME_DELETED || entry.name[0] == '.') continue;
        
        if (DIR_IS_FILE(&entry)) {
            totalSize += entry.fileSize;
        } else if (DIR_IS_SUBDIR(&entry)) {
            char filename[13];
            dir.dirName(entry, filename);
            
            SdFile subdir;
            if (subdir.open(&dir, filename, O_READ)) {
                totalSize += calculateDirectorySize(subdir);
                subdir.close();
            }
        }
    }
    
    return totalSize;
}
